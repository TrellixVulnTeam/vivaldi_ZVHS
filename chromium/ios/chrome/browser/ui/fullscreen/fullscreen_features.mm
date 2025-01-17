// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"

#import <Foundation/Foundation.h>
#include "base/command_line.h"
#include "base/feature_list.h"

// Vivaldi
#import <UIKit/UIKit.h>
#include "app/vivaldi_apptools.h"
using vivaldi::IsVivaldiRunning;
// End Vivaldi

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace fullscreen {
namespace features {

const base::Feature kSmoothScrollingDefault{"FullscreenSmoothScrollingDefault",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

bool ShouldUseSmoothScrolling() {

  // Vivaldi
  // The smooth scrolling settings doesn't affect the user experience on iPhone
  // on our internal testing.
  // But, it's causing a bug where the scrolling to top on web pages leaves a
  // blank space above the webpage. Therefore, we are disabling the settings on
  // iPhone.
  if (IsVivaldiRunning() &&
      (UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPhone))
    return false;
  // End Vivaldi

  if (@available(iOS 16, *)) {
    static bool disable_smooth_scrolling = [[[NSBundle mainBundle]
        bundleIdentifier] hasPrefix:@"org.chromium.ost.chrome"];
    return !disable_smooth_scrolling &&
           base::FeatureList::IsEnabled(kSmoothScrollingDefault);
  }
  return base::FeatureList::IsEnabled(kSmoothScrollingDefault);
}

}  // namespace features
}  // namespace fullscreen
