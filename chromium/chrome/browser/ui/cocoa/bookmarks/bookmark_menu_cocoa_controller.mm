// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller.h"

#import "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"  // IDC_BOOKMARK_MENU
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/profile_metrics/browser_profile_type.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/menu_controller.h"

#include "app/vivaldi_apptools.h"
#include "components/prefs/pref_service.h"
#include "ui/vivaldi_bookmark_menu_mac.h"

using base::UserMetricsAction;
using bookmarks::BookmarkNode;
using content::OpenURLParams;
using content::Referrer;

namespace {

// Returns the NSMenuItem in |submenu|'s supermenu that holds |submenu|.
NSMenuItem* GetItemWithSubmenu(NSMenu* submenu) {
  NSArray* parent_items = [[submenu supermenu] itemArray];
  for (NSMenuItem* item in parent_items) {
    if ([item submenu] == submenu)
      return item;
  }
  return nil;
}

}  // namespace

@implementation BookmarkMenuCocoaController {
 @private
  raw_ptr<BookmarkMenuBridge> _bridge;  // Weak. Owns |self|.
}

+ (NSString*)tooltipForNode:(const BookmarkNode*)node {
  NSString* title = base::SysUTF16ToNSString(node->GetTitle());
  if (node->is_folder())
    return title;
  std::string urlString = node->url().possibly_invalid_spec();
  NSString* url = base::SysUTF8ToNSString(urlString);
  return cocoa_l10n_util::TooltipForURLAndTitle(url, title);
}

- (instancetype)initWithBridge:(BookmarkMenuBridge*)bridge {
  if ((self = [super init])) {
    _bridge = bridge;
    DCHECK(_bridge);
  }
  return self;
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  AppController* controller =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
  return ![controller keyWindowIsModal];
}

// NSMenu delegate method: called just before menu is displayed.
- (void)menuNeedsUpdate:(NSMenu*)menu {
  NSMenuItem* item = GetItemWithSubmenu(menu);
  const BookmarkNode* node = [self nodeForIdentifier:[item tag]];
  _bridge->UpdateMenu(menu, node);
}

- (BOOL)menuHasKeyEquivalent:(NSMenu*)menu
                    forEvent:(NSEvent*)event
                      target:(id*)target
                      action:(SEL*)action {
  // Note it is OK to return NO if there's already an item in |menu| that
  // handles |event|.
  return NO;
}

// Return the a BookmarkNode that has the given id (called
// "identifier" here to avoid conflict with objc's concept of "id").
- (const BookmarkNode*)nodeForIdentifier:(int)identifier {
  return bookmarks::GetBookmarkNodeByID(_bridge->GetBookmarkModel(),
                                        identifier);
}

// Open the URL of the given BookmarkNode in the current tab.
- (void)openURLForNode:(const BookmarkNode*)node {
  Browser* browser = chrome::FindTabbedBrowser(_bridge->GetProfile(), true);
  if (!browser) {
    browser =
        Browser::Create(Browser::CreateParams(_bridge->GetProfile(), true));
  }

  WindowOpenDisposition disposition =
      ui::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  if (vivaldi::IsVivaldiRunning()) {
    disposition = vivaldi::WindowOpenDispositionFromNSEvent(
        [NSApp currentEvent], _bridge->GetProfile()->GetPrefs());
  }

  OpenURLParams params(
      node->url(), Referrer(), disposition,
      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
  browser->OpenURL(params);
  RecordBookmarkLaunch(
      BookmarkLaunchLocation::kTopMenu,
      profile_metrics::GetBrowserProfileType(_bridge->GetProfile()));
}

- (IBAction)openBookmarkMenuItem:(id)sender {
  NSInteger tag = [sender tag];
  int identifier = tag;
  const BookmarkNode* node = [self nodeForIdentifier:identifier];
  DCHECK(node);
  if (!node)
    return;  // shouldn't be reached

  [self openURLForNode:node];
}

@end  // BookmarkMenuCocoaController
