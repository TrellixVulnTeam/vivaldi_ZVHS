// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/importer/importer_lock_dialog.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/user_metrics.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "base/strings/sys_string_conversions.h"

using base::UserMetricsAction;

namespace importer {

void ShowImportLockDialog(gfx::NativeWindow parent,
                          const base::Callback<void(bool)>& callback,
                           base::string16 importer_locktext) {
  base::scoped_nsobject<NSAlert> lock_alert([[NSAlert alloc] init]);
  [lock_alert addButtonWithTitle:l10n_util::GetNSStringWithFixup(
      IDS_IMPORTER_LOCK_OK)];
  [lock_alert addButtonWithTitle:l10n_util::GetNSStringWithFixup(
      IDS_IMPORTER_LOCK_CANCEL)];
  [lock_alert setInformativeText:base::SysUTF16ToNSString(importer_locktext)];
  [lock_alert setMessageText:l10n_util::GetNSStringWithFixup(
      IDS_IMPORTER_LOCK_TITLE)];

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, [lock_alert runModal] == NSAlertFirstButtonReturn));
  content::RecordAction(UserMetricsAction("ImportLockDialogCocoa_Shown"));
}

}  // namespace importer
