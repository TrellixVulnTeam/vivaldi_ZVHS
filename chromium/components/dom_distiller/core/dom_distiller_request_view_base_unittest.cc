// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "components/dom_distiller/core/article_distillation_update.h"
#include "components/dom_distiller/core/test_request_view_handle.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "grit/components_resources.h"
#include "grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::HasSubstr;
using testing::Not;

namespace dom_distiller {

class DomDistillerRequestViewTest : public testing::Test {
 protected:
  void SetUp() override {
    pref_service_.reset(new user_prefs::TestingPrefServiceSyncable());
    DistilledPagePrefs::RegisterProfilePrefs(pref_service_->registry());
    distilled_page_prefs_.reset(new DistilledPagePrefs(pref_service_.get()));
  }

  scoped_ptr<user_prefs::TestingPrefServiceSyncable> pref_service_;
  scoped_ptr<DistilledPagePrefs> distilled_page_prefs_;
};

TEST_F(DomDistillerRequestViewTest, TestTitleEscaped) {
  const std::string no_title =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_NO_DATA_TITLE);
  const std::string valid_title = "valid title";
  const std::string has_quotes = "\"" + valid_title + "\"";
  const std::string escaped_quotes = "\\\"" + valid_title + "\\\"";
  const std::string has_special_chars = "<" + valid_title + "\\";
  const std::string escaped_special_chars = "\\u003C" + valid_title + "\\\\";

  TestRequestViewHandle handle(distilled_page_prefs_.get());

  // Make sure title is properly escaped from quotes.
  {
    scoped_ptr<DistilledArticleProto> article_proto(
        new DistilledArticleProto());
    article_proto->set_title(has_quotes);

    handle.OnArticleReady(article_proto.get());

    EXPECT_THAT(handle.GetJavaScriptBuffer(), HasSubstr(escaped_quotes));
    EXPECT_THAT(handle.GetJavaScriptBuffer(), Not(HasSubstr(has_quotes)));
    handle.ClearJavaScriptBuffer();
  }

  // Make sure title is properly escaped from special characters.
  {
    scoped_ptr<DistilledArticleProto> article_proto(
        new DistilledArticleProto());
    article_proto->set_title(has_special_chars);

    handle.OnArticleReady(article_proto.get());

    EXPECT_THAT(handle.GetJavaScriptBuffer(), HasSubstr(escaped_special_chars));
    EXPECT_THAT(handle.GetJavaScriptBuffer(),
                Not(HasSubstr(has_special_chars)));
    handle.ClearJavaScriptBuffer();
  }

}

TEST_F(DomDistillerRequestViewTest, TestTitleNeverEmpty) {
  const std::string no_title =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_NO_DATA_TITLE);
  const std::string valid_title = "valid title";

  TestRequestViewHandle handle(distilled_page_prefs_.get());

  // Test that the title actually gets shown.
  {
    scoped_ptr<DistilledArticleProto> article_proto(
        new DistilledArticleProto());
    article_proto->set_title(valid_title);

    handle.OnArticleReady(article_proto.get());

    EXPECT_THAT(handle.GetJavaScriptBuffer(), HasSubstr(valid_title));
    EXPECT_THAT(handle.GetJavaScriptBuffer(), Not(HasSubstr(no_title)));
    handle.ClearJavaScriptBuffer();
  }

  // Test empty string title
  {
    scoped_ptr<DistilledArticleProto> article_proto(
        new DistilledArticleProto());
    article_proto->set_title("");

    handle.OnArticleReady(article_proto.get());

    EXPECT_THAT(handle.GetJavaScriptBuffer(), HasSubstr(no_title));
    EXPECT_THAT(handle.GetJavaScriptBuffer(), Not(HasSubstr(valid_title)));
    handle.ClearJavaScriptBuffer();
  }

  // Test no title.
  {
    scoped_ptr<DistilledArticleProto> article_proto(
        new DistilledArticleProto());

    handle.OnArticleReady(article_proto.get());

    EXPECT_THAT(handle.GetJavaScriptBuffer(), HasSubstr(no_title));
    EXPECT_THAT(handle.GetJavaScriptBuffer(), Not(HasSubstr(valid_title)));
    handle.ClearJavaScriptBuffer();
  }
}

TEST_F(DomDistillerRequestViewTest, TestContentNeverEmpty) {
  const std::string no_content =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_NO_DATA_CONTENT);
  const std::string valid_content = "valid content";

  TestRequestViewHandle handle(distilled_page_prefs_.get());

  // Test single page content
  {
    scoped_ptr<DistilledArticleProto> article_proto(
        new DistilledArticleProto());
    (*(article_proto->add_pages())).set_html(valid_content);

    handle.OnArticleReady(article_proto.get());

    EXPECT_THAT(handle.GetJavaScriptBuffer(), HasSubstr(valid_content));
    EXPECT_THAT(handle.GetJavaScriptBuffer(), Not(HasSubstr(no_content)));
    handle.ClearJavaScriptBuffer();
  }

  // Test multiple page content
  {
    scoped_ptr<DistilledArticleProto> article_proto(
        new DistilledArticleProto());
    (*(article_proto->add_pages())).set_html(valid_content);
    (*(article_proto->add_pages())).set_html(valid_content);

    handle.OnArticleReady(article_proto.get());

    EXPECT_THAT(handle.GetJavaScriptBuffer(),
                HasSubstr(valid_content + valid_content));
    EXPECT_THAT(handle.GetJavaScriptBuffer(), Not(HasSubstr(no_content)));
    handle.ClearJavaScriptBuffer();
  }

  // Test empty string content
  {
    scoped_ptr<DistilledArticleProto> article_proto(
        new DistilledArticleProto());
    (*(article_proto->add_pages())).set_html("");

    handle.OnArticleReady(article_proto.get());

    EXPECT_THAT(handle.GetJavaScriptBuffer(), HasSubstr(no_content));
    EXPECT_THAT(handle.GetJavaScriptBuffer(), Not(HasSubstr(valid_content)));
    handle.ClearJavaScriptBuffer();
  }

  // Test page no content
  {
    scoped_ptr<DistilledArticleProto> article_proto(
        new DistilledArticleProto());
    article_proto->add_pages();

    handle.OnArticleReady(article_proto.get());

    EXPECT_THAT(handle.GetJavaScriptBuffer(), HasSubstr(no_content));
    EXPECT_THAT(handle.GetJavaScriptBuffer(), Not(HasSubstr(valid_content)));
    handle.ClearJavaScriptBuffer();
  }

  // Test no page.
  {
    scoped_ptr<DistilledArticleProto> article_proto(
        new DistilledArticleProto());

    handle.OnArticleReady(article_proto.get());

    EXPECT_THAT(handle.GetJavaScriptBuffer(), HasSubstr(no_content));
    EXPECT_THAT(handle.GetJavaScriptBuffer(), Not(HasSubstr(valid_content)));
    handle.ClearJavaScriptBuffer();
  }

  // Test empty string page update
  {
    std::vector<scoped_refptr<ArticleDistillationUpdate::RefCountedPageProto>>
        pages;
    scoped_refptr<base::RefCountedData<DistilledPageProto>> page_proto =
        new base::RefCountedData<DistilledPageProto>();
    page_proto->data.set_html("");
    pages.push_back(page_proto);

    scoped_ptr<ArticleDistillationUpdate> article_update(
        new ArticleDistillationUpdate(pages, false, false));

    handle.OnArticleUpdated(*article_update.get());

    EXPECT_THAT(handle.GetJavaScriptBuffer(), HasSubstr(no_content));
    EXPECT_THAT(handle.GetJavaScriptBuffer(), Not(HasSubstr(valid_content)));
    handle.ClearJavaScriptBuffer();
  }
}

TEST_F(DomDistillerRequestViewTest, TestLoadingIndicator) {
  const std::string no_content =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_NO_DATA_CONTENT);
  // Showing the indicator does mean passing 'false' as the parameter.
  const std::string show_loader = "showLoadingIndicator(false);";

  TestRequestViewHandle handle(distilled_page_prefs_.get());
  handle.TakeViewerHandle(NULL);

  // The loading indicator should show before any content is displayed.
  EXPECT_THAT(handle.GetJavaScriptBuffer(), HasSubstr(show_loader));
  handle.ClearJavaScriptBuffer();

  std::vector<scoped_refptr<ArticleDistillationUpdate::RefCountedPageProto>>
      pages;
  scoped_refptr<base::RefCountedData<DistilledPageProto>> page_proto =
      new base::RefCountedData<DistilledPageProto>();
  pages.push_back(page_proto);

  scoped_ptr<ArticleDistillationUpdate> article_update(
      new ArticleDistillationUpdate(pages, true, false));

  handle.OnArticleUpdated(*article_update.get());

  EXPECT_THAT(handle.GetJavaScriptBuffer(), HasSubstr(show_loader));
}

}  // namespace dom_distiller
