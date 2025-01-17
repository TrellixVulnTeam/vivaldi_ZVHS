// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/guest_view_base.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "components/guest_view/browser/guest_view_event.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/page/page_zoom.h"

#ifdef VIVALDI_BUILD
#include "app/vivaldi_apptools.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "ui/content/vivaldi_tab_check.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/helper/vivaldi_app_helper.h"
#endif
#endif //VIVALDI_BUILD

using content::WebContents;

namespace guest_view {

namespace {

using WebContentsGuestViewMap = std::map<const WebContents*, GuestViewBase*>;
base::LazyInstance<WebContentsGuestViewMap>::Leaky g_webcontents_guestview_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

SetSizeParams::SetSizeParams() = default;
SetSizeParams::~SetSizeParams() = default;

// TODO(832879): It would be better to have proper ownership semantics than
// manually destroying guests and their WebContents.
//
// This observer ensures that the GuestViewBase destroys itself when its
// embedder goes away. It also tracks when the embedder's fullscreen is
// toggled so the guest can change itself accordingly.
class GuestViewBase::OwnerContentsObserver : public WebContentsObserver {
 public:
  OwnerContentsObserver(GuestViewBase* guest,
                        WebContents* embedder_web_contents)
      : WebContentsObserver(embedder_web_contents),
        is_fullscreen_(false),
        destroyed_(false),
        guest_(guest) {}

  OwnerContentsObserver(const OwnerContentsObserver&) = delete;
  OwnerContentsObserver& operator=(const OwnerContentsObserver&) = delete;

  ~OwnerContentsObserver() override = default;

  // WebContentsObserver implementation.
  void WebContentsDestroyed() override {
    // If the embedder is destroyed then destroy the guest.
    Destroy();
  }

  void PrimaryPageChanged(content::Page& page) override {
    // TODO(1206312, 1205920): It is incorrect to assume that a navigation will
    // destroy the embedder.
    // If the embedder navigates to a different page then destroy the guest.
    Destroy();
  }

  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    if (destroyed_)
      return;
    // If the embedder process is destroyed, then destroy the guest.
    Destroy();
  }

  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override {
    if (destroyed_)
      return;

    is_fullscreen_ = entered_fullscreen;
    guest_->EmbedderFullscreenToggled(is_fullscreen_);
  }

  void PrimaryMainFrameWasResized(bool width_changed) override {
    if (destroyed_ || !web_contents()->GetDelegate())
      return;

    bool current_fullscreen =
        web_contents()->GetDelegate()->IsFullscreenForTabOrPending(
            web_contents());
    if (is_fullscreen_ && !current_fullscreen) {
      is_fullscreen_ = false;
      guest_->EmbedderFullscreenToggled(is_fullscreen_);
    }
  }

  void DidUpdateAudioMutingState(bool muted) override {
    if (destroyed_)
      return;

    guest_->web_contents()->SetAudioMuted(muted);
  }

  void RenderFrameDeleted(content::RenderFrameHost* rfh) override {
    guest_->OnRenderFrameHostDeleted(rfh->GetProcess()->GetID(),
                                     rfh->GetRoutingID());
  }

 private:
  bool is_fullscreen_;
  bool destroyed_;
  raw_ptr<GuestViewBase> guest_;

  void Destroy() {
    if (destroyed_)
      return;
    destroyed_ = true;

    // The outer WebContents have ownership of attached OOPIF-based guests, so
    // we are not responsible for their deletion.
    bool also_delete = guest_->web_contents() ?
        !guest_->web_contents()->GetOuterWebContents() : false;
    guest_->Destroy(also_delete);
  }
};

// This observer ensures that the GuestViewBase destroys itself if its opener
// WebContents goes away before the GuestViewBase is attached.
class GuestViewBase::OpenerLifetimeObserver : public WebContentsObserver {
 public:
  explicit OpenerLifetimeObserver(GuestViewBase* guest)
      : WebContentsObserver(guest->GetOpener()->web_contents()),
        guest_(guest) {}

  OpenerLifetimeObserver(const OpenerLifetimeObserver&) = delete;
  OpenerLifetimeObserver& operator=(const OpenerLifetimeObserver&) = delete;

  ~OpenerLifetimeObserver() override = default;

  // WebContentsObserver implementation.
  void WebContentsDestroyed() override {
    if (guest_->attached())
      return;

    // If the opener is destroyed then destroy the guest.
    guest_->Destroy(true);
  }

 private:
  raw_ptr<GuestViewBase> guest_;
};

GuestViewBase::GuestViewBase(WebContents* owner_web_contents)
    : owner_web_contents_(owner_web_contents),
      browser_context_(owner_web_contents->GetBrowserContext()),
      guest_instance_id_(GetGuestViewManager()->GetNextInstanceID()),
      view_instance_id_(kInstanceIDNone),
      element_instance_id_(kInstanceIDNone),
      attach_in_progress_(false),
      initialized_(false),
      is_being_destroyed_(false),
      guest_host_(nullptr),
      auto_size_enabled_(false),
      is_full_page_plugin_(false) {
  SetOwnerHost();
}

GuestViewBase::~GuestViewBase() {
  // NOTE(andre@vivaldi.com) : This is only set in Vivaldi. See
  // BrowserPluginGuestDelegate::delegate_to_browser_plugin_.
  if (delegate_to_browser_plugin_) {
    delegate_to_browser_plugin_->set_delegate(nullptr);
  }

  // Make sure destroy is called so the guestview manager is updated.
  // This can happen when the guest is automatically deleted via webcontents
  // being destroyed when attached to a widget. (I.e. an AppWindow.)
  //
  // TODO(igor@vivaldi.com): Do we still need this? And if we do, then
  // investigate if calling Destroy really does what we want. The method calls
  // virtual WillDestroy so WebViewGuest::WillDestroy() will not run when called
  // from the destructor. So perhaps move this to ~WebViewGuest.
  if (web_contents())
    Destroy(true);
}

void GuestViewBase::Init(const base::Value::Dict& create_params,
                         WebContentsCreatedCallback callback) {
  if (initialized_)
    return;
  initialized_ = true;

  if (!GetGuestViewManager()->IsGuestAvailableToContext(this)) {
    // The derived class did not create a WebContents so this class serves no
    // purpose. Let's self-destruct.
    delete this;
    std::move(callback).Run(nullptr);
    return;
  }

  CreateWebContents(create_params,
                    base::BindOnce(&GuestViewBase::CompleteInit,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   create_params.Clone(), std::move(callback)));
}

void GuestViewBase::InitWithWebContents(const base::Value::Dict& create_params,
                                        WebContents* guest_web_contents) {
  DCHECK(guest_web_contents);

  // Create a ZoomController to allow the guest's contents to be zoomed.
  // Do this before adding the GuestView as a WebContents Observer so that
  // the GuestView and its derived classes can re-configure the ZoomController
  // after the latter has handled WebContentsObserver events (observers are
  // notified of events in the same order they are added as observers). For
  // example, GuestViewBase may wish to put its guest into isolated zoom mode
  // in DidFinishNavigation, but since ZoomController always resets to default
  // zoom mode on this event, GuestViewBase would need to do so after
  // ZoomController::DidFinishNavigation has completed.
  zoom::ZoomController::CreateForWebContents(guest_web_contents);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  AttachWebContentsObservers(guest_web_contents);
#endif

  // At this point, we have just created the guest WebContents, we need to add
  // an observer to the owner WebContents. This observer will be responsible
  // for destroying the guest WebContents if the owner goes away.
  owner_contents_observer_ =
      std::make_unique<OwnerContentsObserver>(this, owner_web_contents_);

  WebContentsObserver::Observe(guest_web_contents);

  // NOTE(david@vivaldi.com): In Vivaldi we need to use the
  // DevtoolsConnectorItem as a proxy delegate.
  if (vivaldi::IsVivaldiRunning())
    guest_web_contents->SetDelegate(GetDevToolsConnector());
  else
  guest_web_contents->SetDelegate(this);
  g_webcontents_guestview_map.Get().insert(
      std::make_pair(guest_web_contents, this));
  GetGuestViewManager()->AddGuest(guest_instance_id_, guest_web_contents);

  // Populate the view instance ID if we have it on creation.
  view_instance_id_ =
      create_params.FindInt(kParameterInstanceId).value_or(view_instance_id_);

  SetUpSizing(create_params);

  // Observe guest zoom changes.
  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents());
  zoom_controller->AddObserver(this);

  // Give the derived class an opportunity to perform additional initialization.
  DidInitialize(create_params);
}

void GuestViewBase::DispatchOnResizeEvent(const gfx::Size& old_size,
                                          const gfx::Size& new_size) {
  if (new_size == old_size)
    return;

  // Dispatch the onResize event.
  auto args = std::make_unique<base::DictionaryValue>();
  args->SetInteger(kOldWidth, old_size.width());
  args->SetInteger(kOldHeight, old_size.height());
  args->SetInteger(kNewWidth, new_size.width());
  args->SetInteger(kNewHeight, new_size.height());
  DispatchEventToGuestProxy(
      std::make_unique<GuestViewEvent>(kEventResize, std::move(args)));
}

gfx::Size GuestViewBase::GetDefaultSize() const {
  // Setting default size other than viewport makes detached/backgrounded pages
  // to be drawn with a small size when first shown. We do not want this in
  // Vivaldi.
  if (!vivaldi::IsVivaldiRunning() && !is_full_page_plugin())
    return gfx::Size(kDefaultWidth, kDefaultHeight);

  // Full page plugins default to the size of the owner's viewport.
  return owner_web_contents()
      ->GetRenderWidgetHostView()
      ->GetVisibleViewportSize();
}

void GuestViewBase::SetSize(const SetSizeParams& params) {
  bool enable_auto_size =
      params.enable_auto_size ? *params.enable_auto_size : auto_size_enabled_;
  gfx::Size min_size = params.min_size ? *params.min_size : min_auto_size_;
  gfx::Size max_size = params.max_size ? *params.max_size : max_auto_size_;

  if (params.normal_size)
    normal_size_ = *params.normal_size;

  min_auto_size_ = min_size;
  min_auto_size_.SetToMin(max_size);
  max_auto_size_ = max_size;
  max_auto_size_.SetToMax(min_size);

  enable_auto_size &= !min_auto_size_.IsEmpty() && !max_auto_size_.IsEmpty() &&
                      IsAutoSizeSupported();

  content::RenderWidgetHostView* rwhv =
      web_contents()->GetRenderWidgetHostView();
  if (enable_auto_size) {
    // Autosize is being enabled.
    if (rwhv)
      rwhv->EnableAutoResize(min_auto_size_, max_auto_size_);
    normal_size_.SetSize(0, 0);
  } else {
    // Autosize is being disabled.
    // Use default width/height if missing from partially defined normal size.
    if (normal_size_.width() && !normal_size_.height())
      normal_size_.set_height(GetDefaultSize().height());
    if (!normal_size_.width() && normal_size_.height())
      normal_size_.set_width(GetDefaultSize().width());

    gfx::Size new_size;
    if (!normal_size_.IsEmpty()) {
      new_size = normal_size_;
    } else if (!guest_size_.IsEmpty()) {
      new_size = guest_size_;
    } else {
      new_size = GetDefaultSize();
    }

    bool changed_due_to_auto_resize = false;
    if (auto_size_enabled_) {
      // Autosize was previously enabled.
      if (rwhv)
        rwhv->DisableAutoResize(new_size);
      changed_due_to_auto_resize = true;
    } else {
      // Autosize was already disabled. The RenderWidgetHostView is responsible
      // for the GuestView's size.
    }

    UpdateGuestSize(new_size, changed_due_to_auto_resize);
  }

  auto_size_enabled_ = enable_auto_size;
}

// static
GuestViewBase* GuestViewBase::FromWebContents(const WebContents* web_contents) {
  WebContentsGuestViewMap* guest_map = g_webcontents_guestview_map.Pointer();
  auto it = guest_map->find(web_contents);
  return it == guest_map->end() ? nullptr : it->second;
}

// static
GuestViewBase* GuestViewBase::From(int owner_process_id,
                                   int guest_instance_id) {
  auto* host = content::RenderProcessHost::FromID(owner_process_id);
  if (!host)
    return nullptr;

  WebContents* guest_web_contents =
      GuestViewManager::FromBrowserContext(host->GetBrowserContext())
          ->GetGuestByInstanceIDSafely(guest_instance_id, owner_process_id);
  if (!guest_web_contents)
    return nullptr;

  return GuestViewBase::FromWebContents(guest_web_contents);
}

// static
WebContents* GuestViewBase::GetTopLevelWebContents(WebContents* web_contents) {
  while (GuestViewBase* guest = FromWebContents(web_contents))
    web_contents = guest->owner_web_contents();
  return web_contents;
}

// static
bool GuestViewBase::IsGuest(WebContents* web_contents) {
  return !!GuestViewBase::FromWebContents(web_contents);
}

bool GuestViewBase::IsAutoSizeSupported() const {
  return false;
}

bool GuestViewBase::IsPreferredSizeModeEnabled() const {
  return false;
}

bool GuestViewBase::ZoomPropagatesFromEmbedderToGuest() const {
  return true;
}

GuestViewManager* GuestViewBase::GetGuestViewManager() {
  return GuestViewManager::FromBrowserContext(browser_context());
}

WebContents* GuestViewBase::CreateNewGuestWindow(
    const WebContents::CreateParams& create_params) {
  if( !owner_web_contents())
    return nullptr;

  return GetGuestViewManager()->CreateGuestWithWebContentsParams(
      GetViewType(), owner_web_contents(), create_params);
}

void GuestViewBase::OnRenderFrameHostDeleted(int process_id, int routing_id) {}

void GuestViewBase::DidAttach() {
  DCHECK(attach_in_progress_);
  // Clear this flag here, as functions called below may check attached().
  attach_in_progress_ = false;

  opener_lifetime_observer_.reset();

  SetUpSizing(attach_params());

  // NOTE(andre@vivaldi.com) : We can set muting on a tab, which is a guest. So
  // we cannot do the default behaviour of copying the parents mute-state. Check
  // bail if the owner is our app-window. The guest should have the same muting
  if (!extensions::VivaldiAppHelper::FromWebContents(owner_web_contents())){
  // state as the owner.
  web_contents()->SetAudioMuted(owner_web_contents()->IsAudioMuted());
  } // if the owner is the Vivaldi app-window.

  // Give the derived class an opportunity to perform some actions.
  DidAttachToEmbedder();

  SendQueuedEvents();
}

WebContents* GuestViewBase::GetOwnerWebContents() {
  return owner_web_contents_;
}

const GURL& GuestViewBase::GetOwnerSiteURL() const {
  return owner_web_contents()
      ->GetPrimaryMainFrame()
      ->GetSiteInstance()
      ->GetSiteURL();
}

void GuestViewBase::Destroy(bool also_delete) {
  if (is_being_destroyed_)
    return;

  is_being_destroyed_ = true;

  // It is important to clear owner_web_contents_ after the call to
  // StopTrackingEmbedderZoomLevel(), but before the rest of
  // the statements in this function.
  StopTrackingEmbedderZoomLevel();
  owner_web_contents_ = nullptr;

  DCHECK(web_contents());

  // Give the derived class an opportunity to perform some cleanup.
  WillDestroy();

  // Invalidate weak pointers now so that bound callbacks cannot be called late
  // into destruction. We must call this after WillDestroy because derived types
  // may wish to access their openers.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Give the content module an opportunity to perform some cleanup.
  if (guest_host_) {
    // clang-format off
  guest_host_->WillDestroy();
    // clang-format on
  }
  guest_host_ = nullptr;

  g_webcontents_guestview_map.Get().erase(web_contents());
  GetGuestViewManager()->RemoveGuest(guest_instance_id_);
  pending_events_.clear();

  if (vivaldi::IsVivaldiRunning() && web_contents()) {
    if (VivaldiTabCheck::IsOwnedByTabStripOrDevTools(web_contents())) {
      web_contents()->SetDelegate(nullptr);
      content::WebContentsObserver::Observe(nullptr);
      return;
    }
    // NOTE(jarle@vivaldi): Check if the WebContent object is already being
    // destroyed. We can get a callback from the WebContentsImpl destructor
    // through GuestViewBase::OwnerContentsObserver. This fixes VB-8381.
    //
    // NOTE(igor@vivaldi.com): This check is probably not applicable. The
    // IsOwnedByTabStripOrDevTools check should reliably skip externally owned
    // WebContents and GuestViewBase::WebContentsDestroyed() calls
    // Destroy(false). But there could still be a code path that destroys the
    // view from another WebContentsDestroyed callback before Destroy(false) was
    // called. As we patched the destructor to call Destroy(true) we need to
    // check for that possibility.
    if (web_contents()->IsBeingDestroyed())
      return;
  }

  if (also_delete)
    delete web_contents();
}

void GuestViewBase::SetAttachParams(const base::Value::Dict& params) {
  attach_params_ = params.Clone();
  view_instance_id_ =
      attach_params_.FindInt(kParameterInstanceId).value_or(view_instance_id_);
}

void GuestViewBase::SetOpener(GuestViewBase* guest) {
  if (guest) {
    opener_ = guest->weak_ptr_factory_.GetWeakPtr();
    if (!attached()) {
      opener_lifetime_observer_ =
          std::make_unique<OpenerLifetimeObserver>(this);
    }
  } else {
    opener_ = base::WeakPtr<GuestViewBase>();
    opener_lifetime_observer_.reset();
  }
}

void GuestViewBase::SetGuestHost(content::GuestHost* guest_host) {
  guest_host_ = guest_host;
}

void GuestViewBase::WillAttach(WebContents* embedder_web_contents,
                               int element_instance_id,
                               bool is_full_page_plugin,
                               base::OnceClosure completion_callback) {
  WillAttach(embedder_web_contents, nullptr, element_instance_id,
             is_full_page_plugin, std::move(completion_callback),
             base::NullCallback());
}

void GuestViewBase::WillAttach(
    WebContents* embedder_web_contents,
    content::RenderFrameHost* outer_contents_frame,
    int element_instance_id,
    bool is_full_page_plugin,
    base::OnceClosure completion_callback,
    GuestViewMessageHandler::AttachToEmbedderFrameCallback
        attachment_callback) {
  // Stop tracking the old embedder's zoom level.
  if (owner_web_contents())
    StopTrackingEmbedderZoomLevel();

  if (owner_web_contents_ != embedder_web_contents) {
    DCHECK_EQ(owner_contents_observer_->web_contents(), owner_web_contents_);
    owner_web_contents_ = embedder_web_contents;
    owner_contents_observer_ =
        std::make_unique<OwnerContentsObserver>(this, embedder_web_contents);
    SetOwnerHost();
  }

  // Start tracking the new embedder's zoom level.
  StartTrackingEmbedderZoomLevel();
  attach_in_progress_ = true;
  element_instance_id_ = element_instance_id;
  is_full_page_plugin_ = is_full_page_plugin;

  WillAttachToEmbedder();

  web_contents()->ResumeLoadingCreatedWebContents();

  // Since this inner WebContents is created from the browser side we do
  // not have RemoteFrame mojo channels so we pass in
  // NullAssociatedRemote/Receivers. New channels will be bound when the
  // `CreateView` IPC is sent.
  owner_web_contents_->AttachInnerWebContents(
      base::WrapUnique<WebContents>(web_contents()), outer_contents_frame,
      /*remote_frame=*/mojo::NullAssociatedRemote(),
      /*remote_frame_host_receiver=*/mojo::NullAssociatedReceiver(),
      is_full_page_plugin);
  // We don't ACK until after AttachToOuterWebContentsFrame, so that
  // |outer_contents_frame| gets swapped before the AttachToEmbedderFrame
  // callback is run. We also need to send the ACK before queued events are sent
  // in DidAttach.
  if (attachment_callback)
    std::move(attachment_callback).Run();

  // Completing attachment will resume suspended resource loads and then send
  // queued events.
  SignalWhenReady(std::move(completion_callback));
}

void GuestViewBase::SignalWhenReady(base::OnceClosure callback) {
  // The default behavior is to call the |callback| immediately. Derived classes
  // can implement an alternative signal for readiness.
  std::move(callback).Run();
}

int GuestViewBase::LogicalPixelsToPhysicalPixels(double logical_pixels) const {
  DCHECK(logical_pixels >= 0);
  double zoom_factor = GetEmbedderZoomFactor();
  return lround(logical_pixels * zoom_factor);
}

double GuestViewBase::PhysicalPixelsToLogicalPixels(int physical_pixels) const {
  DCHECK(physical_pixels >= 0);
  double zoom_factor = GetEmbedderZoomFactor();
  return physical_pixels / zoom_factor;
}

void GuestViewBase::DidStopLoading() {
  content::RenderViewHost* rvh =
      web_contents()->GetPrimaryMainFrame()->GetRenderViewHost();

  if (IsPreferredSizeModeEnabled())
    rvh->EnablePreferredSizeMode();
  GuestViewDidStopLoading();
}

void GuestViewBase::RenderViewReady() {
  GuestReady();
}

void GuestViewBase::WebContentsDestroyed() {
  Destroy(false);

  // Let the derived class know that its WebContents is in the process of
  // being destroyed. web_contents() is still valid at this point.
  // TODO(fsamuel): This allows for reentrant code into WebContents during
  // destruction. This could potentially lead to bugs. Perhaps we should get rid
  // of this?
  GuestDestroyed();

  // Self-destruct.
  delete this;
}

void GuestViewBase::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(crbug.com/1261928): Due to the use of inner WebContents, a
  // GuestViewBase's main frame is considered primary. This will no
  // longer be the case once we migrate guest views to MPArch.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted())
    return;

  if (attached() && ZoomPropagatesFromEmbedderToGuest())
    SetGuestZoomLevelToMatchEmbedder();
}

void GuestViewBase::ActivateContents(WebContents* web_contents) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return;

  embedder_web_contents()->GetDelegate()->ActivateContents(
      embedder_web_contents());
}

void GuestViewBase::ContentsMouseEvent(WebContents* source,
                                       bool motion,
                                       bool exited) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return;

  embedder_web_contents()->GetDelegate()->ContentsMouseEvent(
      embedder_web_contents(), motion, exited);
}

void GuestViewBase::ContentsZoomChange(bool zoom_in) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return;
  embedder_web_contents()->GetDelegate()->ContentsZoomChange(zoom_in);
}

bool GuestViewBase::HandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return false;

  // Send the keyboard events back to the embedder to reprocess them.
  return embedder_web_contents()->GetDelegate()->HandleKeyboardEvent(
      embedder_web_contents(), event);
}

void GuestViewBase::LoadingStateChanged(WebContents* source,
                                        bool should_show_loading_ui) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return;

  embedder_web_contents()->GetDelegate()->LoadingStateChanged(
      embedder_web_contents(), should_show_loading_ui);
}

void GuestViewBase::ResizeDueToAutoResize(WebContents* web_contents,
                                          const gfx::Size& new_size) {
  UpdateGuestSize(new_size, auto_size_enabled_);
}

void GuestViewBase::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  if (!attached() || !embedder_web_contents()->GetDelegate()) {
    listener->FileSelectionCanceled();
    return;
  }

  embedder_web_contents()->GetDelegate()->RunFileChooser(
      render_frame_host, std::move(listener), params);
}

bool GuestViewBase::ShouldFocusPageAfterCrash() {
  // Focus is managed elsewhere.
  return false;
}

bool GuestViewBase::PreHandleGestureEvent(WebContents* source,
                                          const blink::WebGestureEvent& event) {
  if (vivaldi::IsVivaldiRunning()) {
    // NOTE(espen@vivaldi.com): We need pinch events in guests. DCHECK below
    // is not correct for us.
    return false;
  }
  // Pinch events which cause a scale change should not be routed to a guest.
  // We still allow synthetic wheel events for touchpad pinch to go to the page.
  DCHECK(!blink::WebInputEvent::IsPinchGestureEventType(event.GetType()) ||
         (event.SourceDevice() == blink::WebGestureDevice::kTouchpad &&
          event.NeedsWheelEvent()));
  return false;
}

void GuestViewBase::UpdatePreferredSize(WebContents* target_web_contents,
                                        const gfx::Size& pref_size) {
  // In theory it's not necessary to check IsPreferredSizeModeEnabled() because
  // there will only be events if it was enabled in the first place. However,
  // something else may have turned on preferred size mode, so double check.
  DCHECK_EQ(web_contents(), target_web_contents);
  if (IsPreferredSizeModeEnabled()) {
    OnPreferredSizeChanged(pref_size);
  }
}

content::WebContents* GuestViewBase::GetResponsibleWebContents(
    content::WebContents* source) {
  return owner_web_contents();
}

void GuestViewBase::UpdateTargetURL(WebContents* source, const GURL& url) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return;

  embedder_web_contents()->GetDelegate()->UpdateTargetURL(
      embedder_web_contents(), url);
}

bool GuestViewBase::ShouldResumeRequestsForCreatedWindow() {
  // Delay so that the embedder page has a chance to call APIs such as
  // webRequest in time to be applied to the initial navigation in the new guest
  // contents. We resume during WillAttach.
  return false;
}

content::RenderWidgetHost* GuestViewBase::GetOwnerRenderWidgetHost() {
  // We assume guests live inside an owner RenderFrame but the RenderFrame may
  // not be cross-process. In case a type of guest should be allowed to be
  // embedded in a cross-process frame, this method should be overrode for that
  // specific guest type. For all other guests, the owner RenderWidgetHost is
  // that of the owner WebContents.
  DCHECK(!CanBeEmbeddedInsideCrossProcessFrames());
  auto* owner = GetOwnerWebContents();
  if (owner && owner->GetRenderWidgetHostView())
    return owner->GetRenderWidgetHostView()->GetRenderWidgetHost();
  return nullptr;
}

content::SiteInstance* GuestViewBase::GetOwnerSiteInstance() {
  // We assume guests live inside an owner RenderFrame but the RenderFrame may
  // not be cross-process. In case a type of guest should be allowed to be
  // embedded in a cross-process frame, this method should be overrode for that
  // specific guest type. For all other guests, the owner site instance can be
  // from the owner WebContents.
  DCHECK(!CanBeEmbeddedInsideCrossProcessFrames());
  if (auto* owner_contents = GetOwnerWebContents())
    return owner_contents->GetSiteInstance();
  return nullptr;
}

void GuestViewBase::AttachToOuterWebContentsFrame(
    content::RenderFrameHost* embedder_frame,
    int32_t element_instance_id,
    bool is_full_page_plugin,
    GuestViewMessageHandler::AttachToEmbedderFrameCallback
        attachment_callback) {
  auto completion_callback =
      base::BindOnce(&GuestViewBase::DidAttach, weak_ptr_factory_.GetWeakPtr());
  WillAttach(WebContents::FromRenderFrameHost(embedder_frame), embedder_frame,
             element_instance_id, is_full_page_plugin,
             std::move(completion_callback), std::move(attachment_callback));
}

void GuestViewBase::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // NOTE(arnar@vivaldi.com): Do not update guest zoom level if embedder
  // is Vivaldi app. (UI Zoom)
  if (embedder_web_contents()) {
    auto *vivaldi_app_helper =
        extensions::VivaldiAppHelper::FromWebContents(embedder_web_contents());
    if (vivaldi_app_helper) {
      return;
    }
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  if (data.web_contents == embedder_web_contents()) {
    // The embedder's zoom level has changed.
    auto* guest_zoom_controller =
        zoom::ZoomController::FromWebContents(web_contents());
    if (blink::PageZoomValuesEqual(data.new_zoom_level,
                                   guest_zoom_controller->GetZoomLevel())) {
      return;
    }
    // When the embedder's zoom level doesn't match the guest's, then update the
    // guest's zoom level to match.
    guest_zoom_controller->SetZoomLevel(data.new_zoom_level);
    return;
  }

  if (data.web_contents == web_contents()) {
    // The guest's zoom level has changed.
    GuestZoomChanged(data.old_zoom_level, data.new_zoom_level);
  }
}

void GuestViewBase::DispatchEventToGuestProxy(
    std::unique_ptr<GuestViewEvent> event) {
  event->Dispatch(this, guest_instance_id_);
}

void GuestViewBase::DispatchEventToView(std::unique_ptr<GuestViewEvent> event) {
  if (attached() && pending_events_.empty()) {
    event->Dispatch(this, view_instance_id_);
    return;
  }
  pending_events_.push_back(std::move(event));
}

void GuestViewBase::SendQueuedEvents() {
  if (!attached())
    return;
  while (!pending_events_.empty()) {
    std::unique_ptr<GuestViewEvent> event_ptr =
        std::move(pending_events_.front());
    pending_events_.pop_front();
    event_ptr->Dispatch(this, view_instance_id_);
  }
}

void GuestViewBase::CompleteInit(base::Value::Dict create_params,
                                 WebContentsCreatedCallback callback,
                                 WebContents* guest_web_contents) {
  if (!guest_web_contents) {
    // The derived class did not create a WebContents so this class serves no
    // purpose. Let's self-destruct.
    delete this;
    std::move(callback).Run(nullptr);
    return;
  }

  // web_contents() will not be set in some cases (like when we avctivate an
  // non-loaded pdf-tab). We can deal with an unset delegate_to_browser_plugin_.
  if (vivaldi::IsVivaldiRunning() && guest_web_contents) {
    delegate_to_browser_plugin_ =
      static_cast<content::WebContentsImpl*>(guest_web_contents)
      ->GetBrowserPluginGuest();
  }

  InitWithWebContents(create_params, guest_web_contents);
  std::move(callback).Run(guest_web_contents);
}

double GuestViewBase::GetEmbedderZoomFactor() const {
  if (!embedder_web_contents())
    return 1.0;

  return blink::PageZoomLevelToZoomFactor(
      zoom::ZoomController::GetZoomLevelForWebContents(
          embedder_web_contents()));
}

void GuestViewBase::SetUpSizing(const base::Value::Dict& params) {
  // Read the autosize parameters passed in from the embedder.
  absl::optional<bool> auto_size_enabled_opt =
      params.FindBool(kAttributeAutoSize);
  bool auto_size_enabled = auto_size_enabled_opt.value_or(auto_size_enabled_);

  int max_height =
      params.FindInt(kAttributeMaxHeight).value_or(max_auto_size_.height());
  int max_width =
      params.FindInt(kAttributeMaxWidth).value_or(max_auto_size_.width());

  int min_height =
      params.FindInt(kAttributeMinHeight).value_or(min_auto_size_.height());
  int min_width =
      params.FindInt(kAttributeMinWidth).value_or(min_auto_size_.width());

  double element_height = params.FindDouble(kElementHeight).value_or(0.0);
  double element_width = params.FindDouble(kElementWidth).value_or(0.0);

  // Set the normal size to the element size so that the guestview will fit
  // the element initially if autosize is disabled.
  int normal_height = normal_size_.height();
  int normal_width = normal_size_.width();
  // If the element size was provided in logical units (versus physical), then
  // it will be converted to physical units.
  absl::optional<bool> element_size_is_logical_opt =
      params.FindBool(kElementSizeIsLogical);
  bool element_size_is_logical = element_size_is_logical_opt.value_or(false);
  if (element_size_is_logical) {
    // Convert the element size from logical pixels to physical pixels.
    normal_height = LogicalPixelsToPhysicalPixels(element_height);
    normal_width = LogicalPixelsToPhysicalPixels(element_width);
  } else {
    normal_height = lround(element_height);
    normal_width = lround(element_width);
  }

  SetSizeParams set_size_params;
  set_size_params.enable_auto_size = std::make_unique<bool>(auto_size_enabled);
  set_size_params.min_size = std::make_unique<gfx::Size>(min_width, min_height);
  set_size_params.max_size = std::make_unique<gfx::Size>(max_width, max_height);
  set_size_params.normal_size =
      std::make_unique<gfx::Size>(normal_width, normal_height);

  // Call SetSize to apply all the appropriate validation and clipping of
  // values.
  SetSize(set_size_params);
}

void GuestViewBase::SetGuestZoomLevelToMatchEmbedder() {
  auto* embedder_zoom_controller =
      zoom::ZoomController::FromWebContents(owner_web_contents());
  if (!embedder_zoom_controller)
    return;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (embedder_web_contents()) {
    auto *vivaldi_app_helper =
      extensions::VivaldiAppHelper::FromWebContents(owner_web_contents());
    if (vivaldi_app_helper) {
      // NOTE(arnar@vivaldi.com): Do not set Guest zoom level to match level of
      // embedder if embedder is Vivaldi app (UI Zoom)
      return;
    }
  }
#endif // BUILDFLAG(ENABLE_EXTENSIONS)

  zoom::ZoomController::FromWebContents(web_contents())
      ->SetZoomLevel(embedder_zoom_controller->GetZoomLevel());
}

void GuestViewBase::StartTrackingEmbedderZoomLevel() {
  if (!ZoomPropagatesFromEmbedderToGuest())
    return;

  auto* embedder_zoom_controller =
      zoom::ZoomController::FromWebContents(owner_web_contents());
  // Chrome Apps do not have a ZoomController.
  if (!embedder_zoom_controller)
    return;
  // Listen to the embedder's zoom changes.
  embedder_zoom_controller->AddObserver(this);

  // Set the guest's initial zoom level to be equal to the embedder's.
  SetGuestZoomLevelToMatchEmbedder();
}

void GuestViewBase::StopTrackingEmbedderZoomLevel() {
  // TODO(wjmaclean): Remove the observer any time the GuestWebView transitions
  // from propagating to not-propagating the zoom from the embedder.

  auto* embedder_zoom_controller =
      zoom::ZoomController::FromWebContents(owner_web_contents());
  // Chrome Apps do not have a ZoomController.
  if (!embedder_zoom_controller)
    return;

  // It is safe to remove an observer that was never registed.
  embedder_zoom_controller->RemoveObserver(this);
}

void GuestViewBase::UpdateGuestSize(const gfx::Size& new_size,
                                    bool due_to_auto_resize) {
  if (due_to_auto_resize)
    GuestSizeChangedDueToAutoSize(guest_size_, new_size);
  DispatchOnResizeEvent(guest_size_, new_size);
  guest_size_ = new_size;
}

void GuestViewBase::SetOwnerHost() {
  auto* manager = GuestViewManager::FromBrowserContext(browser_context_);
  owner_host_ = manager->IsOwnedByExtension(this)
                    ? owner_web_contents()->GetLastCommittedURL().host()
                    : std::string();
}

bool GuestViewBase::CanBeEmbeddedInsideCrossProcessFrames() const {
  return false;
}

content::RenderFrameHost* GuestViewBase::GetGuestMainFrame() const {
  // TODO(crbug/1261928): Migrate the implementation for MPArch.
  return web_contents()->GetPrimaryMainFrame();
}

}  // namespace guest_view
