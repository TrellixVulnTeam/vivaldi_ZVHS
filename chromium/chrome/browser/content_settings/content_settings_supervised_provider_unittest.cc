// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_supervised_provider.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/prefs/testing_pref_store.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace content_settings {

class SupervisedUserProviderTest : public ::testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  SupervisedUserSettingsService service_;
  scoped_refptr<TestingPrefStore> pref_store_;
  scoped_ptr<SupervisedProvider> provider_;
  content_settings::MockObserver mock_observer_;
};

void SupervisedUserProviderTest::SetUp() {
  pref_store_ = new TestingPrefStore();
  pref_store_->NotifyInitializationCompleted();
  service_.Init(pref_store_);
  service_.SetActive(true);
  provider_.reset(new SupervisedProvider(&service_));
  provider_->AddObserver(&mock_observer_);
}

void SupervisedUserProviderTest::TearDown() {
  provider_->RemoveObserver(&mock_observer_);
  provider_->ShutdownOnUIThread();
  service_.Shutdown();
}

TEST_F(SupervisedUserProviderTest, GeolocationTest) {
  scoped_ptr<RuleIterator> rule_iterator(provider_->GetRuleIterator(
      CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(), false));
  EXPECT_FALSE(rule_iterator->HasNext());
  rule_iterator.reset();

  // Disable the default geolocation setting.
  EXPECT_CALL(mock_observer_, OnContentSettingChanged(
                                  _, _, CONTENT_SETTINGS_TYPE_GEOLOCATION, ""));
  service_.SetLocalSetting(
      supervised_users::kGeolocationDisabled,
      scoped_ptr<base::Value>(new base::FundamentalValue(true)));

  rule_iterator.reset(provider_->GetRuleIterator(
      CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(), false));
  ASSERT_TRUE(rule_iterator->HasNext());
  Rule rule = rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());

  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, ValueToContentSetting(rule.value.get()));
  rule_iterator.reset();

  // Re-enable the default geolocation setting.
  EXPECT_CALL(mock_observer_, OnContentSettingChanged(
                                  _, _, CONTENT_SETTINGS_TYPE_GEOLOCATION, ""));
  service_.SetLocalSetting(
      supervised_users::kGeolocationDisabled,
      scoped_ptr<base::Value>(new base::FundamentalValue(false)));

  rule_iterator.reset(provider_->GetRuleIterator(
      CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(), false));
  EXPECT_FALSE(rule_iterator->HasNext());
}

TEST_F(SupervisedUserProviderTest, CameraMicTest) {
  scoped_ptr<RuleIterator> rule_iterator(provider_->GetRuleIterator(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, std::string(), false));
  EXPECT_FALSE(rule_iterator->HasNext());
  rule_iterator.reset();
  rule_iterator.reset(provider_->GetRuleIterator(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, std::string(), false));
  EXPECT_FALSE(rule_iterator->HasNext());
  rule_iterator.reset();

  // Disable the default camera and microphone setting.
  EXPECT_CALL(mock_observer_,
              OnContentSettingChanged(
                  _, _, CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, ""));
  EXPECT_CALL(
      mock_observer_,
      OnContentSettingChanged(_, _, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, ""));
  service_.SetLocalSetting(
      supervised_users::kCameraMicDisabled,
      scoped_ptr<base::Value>(new base::FundamentalValue(true)));

  rule_iterator.reset(provider_->GetRuleIterator(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, std::string(), false));
  ASSERT_TRUE(rule_iterator->HasNext());
  Rule rule = rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());

  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, ValueToContentSetting(rule.value.get()));
  rule_iterator.reset();

  rule_iterator.reset(provider_->GetRuleIterator(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, std::string(), false));
  ASSERT_TRUE(rule_iterator->HasNext());
  rule = rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());

  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, ValueToContentSetting(rule.value.get()));
  rule_iterator.reset();

  // Re-enable the default camera and microphone setting.
  EXPECT_CALL(mock_observer_,
              OnContentSettingChanged(
                  _, _, CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, ""));
  EXPECT_CALL(
      mock_observer_,
      OnContentSettingChanged(_, _, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, ""));
  service_.SetLocalSetting(
      supervised_users::kCameraMicDisabled,
      scoped_ptr<base::Value>(new base::FundamentalValue(false)));

  rule_iterator.reset(provider_->GetRuleIterator(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, std::string(), false));
  EXPECT_FALSE(rule_iterator->HasNext());
  rule_iterator.reset();
  rule_iterator.reset(provider_->GetRuleIterator(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, std::string(), false));
  EXPECT_FALSE(rule_iterator->HasNext());
}

}  // namespace content_settings
