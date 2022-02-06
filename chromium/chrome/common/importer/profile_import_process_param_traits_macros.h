// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or Multiply-included shared traits file depending on circumstances.
// This allows the use of IPC serialization macros in more than one IPC message
// file.
#ifndef CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_MACROS_H_
#define CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_MACROS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/common/common_param_traits_macros.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_autofill_form_data_entry.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/importer/importer_url_row.h"
#include "components/autofill/content/common/autofill_param_traits_macros.h"
#include "components/autofill/core/common/password_form.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"

#include "importer/imported_notes_entry.h"

IPC_ENUM_TRAITS_MIN_MAX_VALUE(importer::ImporterType,
                              importer::TYPE_UNKNOWN,
                              importer::TYPE_OPERA_OPIUM)

IPC_STRUCT_TRAITS_BEGIN(importer::SourceProfile)
  IPC_STRUCT_TRAITS_MEMBER(importer_name)
  IPC_STRUCT_TRAITS_MEMBER(importer_type)
  IPC_STRUCT_TRAITS_MEMBER(source_path)
  IPC_STRUCT_TRAITS_MEMBER(app_path)
  IPC_STRUCT_TRAITS_MEMBER(services_supported)
  IPC_STRUCT_TRAITS_MEMBER(locale)
  IPC_STRUCT_TRAITS_MEMBER(selected_profile_name)// 2015-03-12 added by arnar@vivaldi 
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ImporterURLRow)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(visit_count)
  IPC_STRUCT_TRAITS_MEMBER(typed_count)
  IPC_STRUCT_TRAITS_MEMBER(last_visit)
  IPC_STRUCT_TRAITS_MEMBER(hidden)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ImportedBookmarkEntry)
  IPC_STRUCT_TRAITS_MEMBER(in_toolbar)
  IPC_STRUCT_TRAITS_MEMBER(is_folder)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(path)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(creation_time)
  IPC_STRUCT_TRAITS_MEMBER(nickname)
  IPC_STRUCT_TRAITS_MEMBER(thumbnail)
  IPC_STRUCT_TRAITS_MEMBER(speeddial)
  IPC_STRUCT_TRAITS_MEMBER(description)
  IPC_STRUCT_TRAITS_MEMBER(visited_time)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(favicon_base::FaviconUsageData)
  IPC_STRUCT_TRAITS_MEMBER(favicon_url)
  IPC_STRUCT_TRAITS_MEMBER(png_data)
  IPC_STRUCT_TRAITS_MEMBER(urls)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(importer::SearchEngineInfo)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(keyword)
  IPC_STRUCT_TRAITS_MEMBER(display_name)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ImporterAutofillFormDataEntry)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(times_used)
  IPC_STRUCT_TRAITS_MEMBER(first_used)
  IPC_STRUCT_TRAITS_MEMBER(last_used)
IPC_STRUCT_TRAITS_END()

#if defined(OS_WIN)
IPC_STRUCT_TRAITS_BEGIN(importer::ImporterIE7PasswordInfo)
  IPC_STRUCT_TRAITS_MEMBER(url_hash)
  IPC_STRUCT_TRAITS_MEMBER(encrypted_data)
  IPC_STRUCT_TRAITS_MEMBER(date_created)
IPC_STRUCT_TRAITS_END()
#endif

IPC_STRUCT_TRAITS_BEGIN(importer::ImportConfig)
  IPC_STRUCT_TRAITS_MEMBER(imported_items)
  IPC_STRUCT_TRAITS_MEMBER(arguments)
IPC_STRUCT_TRAITS_END()

#endif  // CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_MACROS_H_
