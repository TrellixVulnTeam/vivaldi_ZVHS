// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_bubble_sign_in_delegate.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/bookmarks/bookmark_bubble_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/range/range.h"

#if defined(OS_CHROMEOS)
const bool kHasProfileChooser = false;
#else
const bool kHasProfileChooser = true;
#endif

class BookmarkBubbleSignInDelegateTest : public InProcessBrowserTest {
 public:
  BookmarkBubbleSignInDelegateTest() {}

  Profile* profile() { return browser()->profile(); }

  void ReplaceBlank(Browser* browser);

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkBubbleSignInDelegateTest);
};

// The default browser created for tests start with one tab open on
// about:blank.  The sign-in page is a singleton that will
// replace this tab.  This function replaces about:blank with another URL
// so that the sign in page goes into a new tab.
void BookmarkBubbleSignInDelegateTest::ReplaceBlank(Browser* browser) {
  chrome::NavigateParams params(
      chrome::GetSingletonTabNavigateParams(browser, GURL("chrome:version")));
  params.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  chrome::ShowSingletonTabOverwritingNTP(browser, params);
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleSignInDelegateTest, OnSignInLinkClicked) {
  ReplaceBlank(browser());
  int starting_tab_count = browser()->tab_strip_model()->count();

  scoped_ptr<BookmarkBubbleDelegate> delegate;
  delegate.reset(new BookmarkBubbleSignInDelegate(browser()));

  delegate->OnSignInLinkClicked();

  if (kHasProfileChooser) {
    EXPECT_TRUE(ProfileChooserView::IsShowing());
    EXPECT_EQ(starting_tab_count, browser()->tab_strip_model()->count());
  } else {
    EXPECT_EQ(starting_tab_count + 1, browser()->tab_strip_model()->count());
  }
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleSignInDelegateTest,
                       OnSignInLinkClickedReusesBlank) {
  int starting_tab_count = browser()->tab_strip_model()->count();

  scoped_ptr<BookmarkBubbleDelegate> delegate;
  delegate.reset(new BookmarkBubbleSignInDelegate(browser()));

  delegate->OnSignInLinkClicked();

  if (kHasProfileChooser) {
    EXPECT_TRUE(ProfileChooserView::IsShowing());
    EXPECT_EQ(starting_tab_count, browser()->tab_strip_model()->count());
  } else {
    EXPECT_EQ(starting_tab_count, browser()->tab_strip_model()->count());
  }
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleSignInDelegateTest,
                       OnSignInLinkClickedIncognito) {
  ReplaceBlank(browser());
  Browser* incognito_browser = CreateIncognitoBrowser();

  int starting_tab_count_normal = browser()->tab_strip_model()->count();
  int starting_tab_count_incognito =
      incognito_browser->tab_strip_model()->count();

  scoped_ptr<BookmarkBubbleDelegate> delegate;
  delegate.reset(new BookmarkBubbleSignInDelegate(incognito_browser));

  delegate->OnSignInLinkClicked();

  if (kHasProfileChooser) {
    // ProfileChooser doesn't show in an incognito window.
    EXPECT_FALSE(ProfileChooserView::IsShowing());
  } else {
    // A new tab should have been opened in the normal browser, which should be
    // visible.
    int tab_count_normal = browser()->tab_strip_model()->count();
    EXPECT_EQ(starting_tab_count_normal + 1, tab_count_normal);
  }
  // No effect is expected on the incognito browser.
  int tab_count_incognito = incognito_browser->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count_incognito, tab_count_incognito);
}

// Verifies that the sign in page can be loaded in a different browser
// if the provided browser is invalidated.
IN_PROC_BROWSER_TEST_F(BookmarkBubbleSignInDelegateTest, BrowserRemoved) {
  // Create an extra browser.
  Browser* extra_browser = CreateBrowser(profile());
  ReplaceBlank(extra_browser);

  int starting_tab_count = extra_browser->tab_strip_model()->count();

  scoped_ptr<BookmarkBubbleDelegate> delegate;
  delegate.reset(new BookmarkBubbleSignInDelegate(browser()));

  BrowserList::SetLastActive(extra_browser);

  // Close all tabs in the original browser.  Run all pending messages
  // to make sure the browser window closes before continuing.
  browser()->tab_strip_model()->CloseAllTabs();
  content::RunAllPendingInMessageLoop();

  delegate->OnSignInLinkClicked();

  if (kHasProfileChooser) {
    EXPECT_TRUE(ProfileChooserView::IsShowing());
  } else {
    // A new tab should have been opened in the extra browser, which should be
    // visible.
    int tab_count = extra_browser->tab_strip_model()->count();
    EXPECT_EQ(starting_tab_count + 1, tab_count);
  }
}
