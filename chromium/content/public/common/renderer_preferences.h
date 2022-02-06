// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A struct for managing browser's settings that apply to the renderer or its
// webview.  These differ from WebPreferences since they apply to Chromium's
// glue layer rather than applying to just WebKit.
//
// Adding new values to this class probably involves updating
// common/view_messages.h, browser/browser.cc, etc.

#ifndef CONTENT_PUBLIC_COMMON_RENDERER_PREFERENCES_H_
#define CONTENT_PUBLIC_COMMON_RENDERER_PREFERENCES_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font_render_params.h"

namespace content {

enum TapMultipleTargetsStrategy {
  TAP_MULTIPLE_TARGETS_STRATEGY_ZOOM = 0,
  TAP_MULTIPLE_TARGETS_STRATEGY_POPUP,
  TAP_MULTIPLE_TARGETS_STRATEGY_NONE,

  TAP_MULTIPLE_TARGETS_STRATEGY_MAX = TAP_MULTIPLE_TARGETS_STRATEGY_NONE,
};

struct CONTENT_EXPORT RendererPreferences {
  RendererPreferences();

  ~RendererPreferences();

  // Whether the renderer's current browser context accept drops from the OS
  // that result in navigations away from the current page.
  bool can_accept_load_drops;

  // Whether text should be antialiased.
  // Currently only used by Linux.
  bool should_antialias_text;

  // The level of hinting to use when rendering text.
  // Currently only used by Linux.
  gfx::FontRenderParams::Hinting hinting;

  // Whether auto hinter should be used. Currently only used by Linux.
  bool use_autohinter;

  // Whether embedded bitmap strikes in fonts should be used.
  // Current only used by Linux.
  bool use_bitmaps;

  // Whether images should be shown or not.
  bool should_show_images;

  // Should plugins be shown after a click. "Click to play."
  // Note: This will override the host-preferences that
  // might be allow, block or ask.
  bool should_ask_plugin_content;

  // The type of subpixel rendering to use for text.
  // Currently only used by Linux and Windows.
  gfx::FontRenderParams::SubpixelRendering subpixel_rendering;

  // Whether subpixel positioning should be used, permitting fractional X
  // positions for glyphs.  Currently only used by Linux.
  bool use_subpixel_positioning;

  // The color of the focus ring. Currently only used on Linux.
  SkColor focus_ring_color;

  // The color of different parts of the scrollbar. Currently only used on
  // Linux.
  SkColor thumb_active_color;
  SkColor thumb_inactive_color;
  SkColor track_color;

  // The colors used in selection text. Currently only used on Linux and Ash.
  SkColor active_selection_bg_color;
  SkColor active_selection_fg_color;
  SkColor inactive_selection_bg_color;
  SkColor inactive_selection_fg_color;

  // Browser wants a look at all non-local top level navigation requests.
  bool browser_handles_non_local_top_level_requests;

  // Browser wants a look at all top-level navigation requests.
  bool browser_handles_all_top_level_requests;

  // Cursor blink rate in seconds.
  // Currently only changed from default on Linux.  Uses |gtk-cursor-blink|
  // from GtkSettings.
  double caret_blink_interval;

  // Whether or not to set custom colors at all.
  bool use_custom_colors;

  // Set to false to not send referrers.
  bool enable_referrers;

  // Set to true to indicate that the preference to set DNT to 1 is enabled.
  bool enable_do_not_track;

  // Set to false to indicate that WebRTC should use the OS default routing.
  bool enable_webrtc_multiple_routes;

  // Default page zoom level.
  double default_zoom_level;

  // The user agent given to WebKit when it requests one and the user agent is
  // being overridden for the current navigation.
  std::string user_agent_override;

  // The accept-languages of the browser, comma-separated.
  std::string accept_languages;

  // Specifies whether the renderer reports frame name changes to the browser
  // process.
  // TODO(fsamuel): This is a short-term workaround to avoid regressing
  // Sunspider. We need to find an efficient way to report changes to frame
  // names to the browser process. See http://crbug.com/169110 for more
  // information.
  bool report_frame_name_changes;

  // How to handle a tap gesture touching multiple targets
  TapMultipleTargetsStrategy tap_multiple_targets_strategy;

  // Disables rendering default error page when client choses to block a page.
  // Corresponds to net::ERR_BLOCKED_BY_CLIENT.
  bool disable_client_blocked_error_page;

  // Determines whether plugins are allowed to enter fullscreen mode.
  bool plugin_fullscreen_allowed;

  // Whether video-overlay (hole-punching) should be used for the embedded
  // encrypted video.  Currently only used by Android.
  bool use_video_overlay_for_embedded_encrypted_video;

  // Use video-overlay (hole-punching) should be used for all video, not just
  // encrypted video.  Currently only used by Android.
  bool use_view_overlay_for_all_video;

  // Country iso of the mobile network for content detection purpose.
  std::string network_contry_iso;

#if defined(OS_WIN)
  // The default system font settings for caption, small caption, menu and
  // status messages. Used only by Windows.
  base::string16 caption_font_family_name;
  int32 caption_font_height;

  base::string16 small_caption_font_family_name;
  int32 small_caption_font_height;

  base::string16 menu_font_family_name;
  int32 menu_font_height;

  base::string16 status_font_family_name;
  int32 status_font_height;

  base::string16 message_font_family_name;
  int32 message_font_height;

  // The width of a vertical scroll bar in dips.
  int32 vertical_scroll_bar_width_in_dips;

  // The height of a horizontal scroll bar in dips.
  int32 horizontal_scroll_bar_height_in_dips;

  // The height of the arrow bitmap on a vertical scroll bar in dips.
  int32 arrow_bitmap_height_vertical_scroll_bar_in_dips;

  // The width of the arrow bitmap on a horizontal scroll bar in dips.
  int32 arrow_bitmap_width_horizontal_scroll_bar_in_dips;
#endif

  bool serve_resources_only_from_cache;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RENDERER_PREFERENCES_H_
