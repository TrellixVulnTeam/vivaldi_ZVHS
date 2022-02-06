// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/importer/importer_uma.h"

namespace importer {

namespace {

// The enum used to register importer use.
enum ImporterTypeMetrics {
  IMPORTER_METRICS_UNKNOWN = 0,
#if defined(OS_WIN)
  IMPORTER_METRICS_IE = 1,
#endif
  IMPORTER_METRICS_FIREFOX2 = 2,  // obsolete
  IMPORTER_METRICS_FIREFOX3 = 3,
#if defined(OS_MACOSX)
  IMPORTER_METRICS_SAFARI          = 4,
#endif
  IMPORTER_METRICS_GOOGLE_TOOLBAR5 = 5,  // obsolete
  IMPORTER_METRICS_BOOKMARKS_FILE = 6,
  IMPORTER_METRICS_OPERA = 7,
  IMPORTER_METRICS_OPERA_BOOKMARK_FILE = 8,
  IMPORTER_METRICS_CHROME = 9,
  IMPORTER_METRICS_VIVALDI = 10,
  IMPORTER_METRICS_YANDEX = 11,
  IMPORTER_METRICS_OPERA_OPIUM = 12,

  // Insert new values here. Never remove any existing values, as this enum is
  // used to bucket a UMA histogram, and removing values breaks that.
  IMPORTER_METRICS_SIZE
};

}  // namespace

void LogImporterUseToMetrics(const std::string& metric_postfix,
                             ImporterType type) {
  ImporterTypeMetrics metrics_type = IMPORTER_METRICS_UNKNOWN;
  switch (type) {
    case TYPE_UNKNOWN:
      metrics_type = IMPORTER_METRICS_UNKNOWN;
      break;
#if defined(OS_WIN)
    case TYPE_IE:
      metrics_type = IMPORTER_METRICS_IE;
      break;
#endif
    case TYPE_FIREFOX:
      metrics_type = IMPORTER_METRICS_FIREFOX3;
      break;
#if defined(OS_MACOSX)
    case TYPE_SAFARI:
      metrics_type = IMPORTER_METRICS_SAFARI;
      break;
#endif
    case TYPE_BOOKMARKS_FILE:
      metrics_type = IMPORTER_METRICS_BOOKMARKS_FILE;
      break;
    case TYPE_OPERA:
      metrics_type = IMPORTER_METRICS_OPERA;
      break;
    case TYPE_OPERA_BOOKMARK_FILE:
      metrics_type = IMPORTER_METRICS_OPERA_BOOKMARK_FILE;
      break;
    case TYPE_CHROME:
      metrics_type = IMPORTER_METRICS_CHROME;
      break;
    case TYPE_VIVALDI:
      metrics_type = IMPORTER_METRICS_VIVALDI;
      break;
    case TYPE_YANDEX:
      metrics_type = IMPORTER_METRICS_YANDEX;
      break;
    case TYPE_OPERA_OPIUM:
    case TYPE_OPERA_OPIUM_BETA:
    case TYPE_OPERA_OPIUM_DEV:
      metrics_type = IMPORTER_METRICS_OPERA_OPIUM;
      break;

  }

  // Note: This leaks memory, which is the expected behavior as the factory
  // creates and owns the histogram.
  base::HistogramBase* histogram =
      base::LinearHistogram::FactoryGet(
          "Import.ImporterType." + metric_postfix,
          1,
          IMPORTER_METRICS_SIZE,
          IMPORTER_METRICS_SIZE + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(metrics_type);
}

}  // namespace importer
