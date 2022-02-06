// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/gcd_rest_client.h"

#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "remoting/host/fake_oauth_token_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class GcdRestClientTest : public testing::Test {
 public:
  GcdRestClientTest()
      : default_token_getter_(OAuthTokenGetter::SUCCESS,
                              "<fake_user_email>",
                              "<fake_access_token>") {}

  void OnRequestComplete(GcdRestClient::Status status) {
    ++counter_;
    last_status_ = status;
    if (delete_client_) {
      client_.reset();
    }
  }

  scoped_ptr<base::DictionaryValue> MakePatchDetails(int id) {
    scoped_ptr<base::DictionaryValue> patch_details(new base::DictionaryValue);
    patch_details->SetInteger("id", id);
    return patch_details.Pass();
  }

  void CreateClient(OAuthTokenGetter* token_getter = nullptr) {
    if (!token_getter) {
      token_getter = &default_token_getter_;
    }
    client_.reset(new GcdRestClient("http://gcd_base_url", "<gcd_device_id>",
                                    nullptr, token_getter));
    client_->SetClockForTest(make_scoped_ptr(new base::SimpleTestClock));
  }

 protected:
  net::TestURLFetcherFactory url_fetcher_factory_;
  FakeOAuthTokenGetter default_token_getter_;
  scoped_ptr<GcdRestClient> client_;
  bool delete_client_ = false;
  int counter_ = 0;
  GcdRestClient::Status last_status_ = GcdRestClient::OTHER_ERROR;

 private:
  base::MessageLoop message_loop_;
};

TEST_F(GcdRestClientTest, NetworkErrorGettingToken) {
  FakeOAuthTokenGetter token_getter(OAuthTokenGetter::NETWORK_ERROR, "", "");
  CreateClient(&token_getter);

  client_->PatchState(MakePatchDetails(0).Pass(),
                      base::Bind(&GcdRestClientTest::OnRequestComplete,
                                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, counter_);
  EXPECT_EQ(GcdRestClient::NETWORK_ERROR, last_status_);
}

TEST_F(GcdRestClientTest, AuthErrorGettingToken) {
  FakeOAuthTokenGetter token_getter(OAuthTokenGetter::AUTH_ERROR, "", "");
  CreateClient(&token_getter);

  client_->PatchState(MakePatchDetails(0).Pass(),
                      base::Bind(&GcdRestClientTest::OnRequestComplete,
                                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, counter_);
  EXPECT_EQ(GcdRestClient::OTHER_ERROR, last_status_);
}

TEST_F(GcdRestClientTest, NetworkErrorOnPost) {
  CreateClient();

  client_->PatchState(MakePatchDetails(0).Pass(),
                      base::Bind(&GcdRestClientTest::OnRequestComplete,
                                 base::Unretained(this)));
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(0);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, counter_);
  EXPECT_EQ(GcdRestClient::NETWORK_ERROR, last_status_);
}

TEST_F(GcdRestClientTest, OtherErrorOnPost) {
  CreateClient();

  client_->PatchState(MakePatchDetails(0).Pass(),
                      base::Bind(&GcdRestClientTest::OnRequestComplete,
                                 base::Unretained(this)));
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(500);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, counter_);
  EXPECT_EQ(GcdRestClient::OTHER_ERROR, last_status_);
}

TEST_F(GcdRestClientTest, NoSuchHost) {
  CreateClient();

  client_->PatchState(MakePatchDetails(0).Pass(),
                      base::Bind(&GcdRestClientTest::OnRequestComplete,
                                 base::Unretained(this)));
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(404);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, counter_);
  EXPECT_EQ(GcdRestClient::NO_SUCH_HOST, last_status_);
}

TEST_F(GcdRestClientTest, Succeed) {
  CreateClient();

  client_->PatchState(MakePatchDetails(0).Pass(),
                      base::Bind(&GcdRestClientTest::OnRequestComplete,
                                 base::Unretained(this)));
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(fetcher);
  EXPECT_EQ("http://gcd_base_url/devices/%3Cgcd_device_id%3E/patchState",
            fetcher->GetOriginalURL().spec());
  EXPECT_EQ(
      "{\"patches\":[{\"patch\":{\"id\":0},\"timeMs\":0.0}],"
      "\"requestTimeMs\":0.0}",
      fetcher->upload_data());
  EXPECT_EQ("application/json", fetcher->upload_content_type());
  fetcher->set_response_code(200);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, counter_);
  EXPECT_EQ(GcdRestClient::SUCCESS, last_status_);
}

TEST_F(GcdRestClientTest, SucceedTwice) {
  CreateClient();

  client_->PatchState(MakePatchDetails(0).Pass(),
                      base::Bind(&GcdRestClientTest::OnRequestComplete,
                                 base::Unretained(this)));
  net::TestURLFetcher* fetcher0 = url_fetcher_factory_.GetFetcherByID(0);
  client_->PatchState(MakePatchDetails(1).Pass(),
                      base::Bind(&GcdRestClientTest::OnRequestComplete,
                                 base::Unretained(this)));
  net::TestURLFetcher* fetcher1 = url_fetcher_factory_.GetFetcherByID(0);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      "{\"patches\":[{\"patch\":{\"id\":0},\"timeMs\":0.0}],"
      "\"requestTimeMs\":0.0}",
      fetcher0->upload_data());
  fetcher0->set_response_code(200);
  fetcher0->delegate()->OnURLFetchComplete(fetcher0);
  EXPECT_EQ(GcdRestClient::SUCCESS, last_status_);
  EXPECT_EQ(1, counter_);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      "{\"patches\":[{\"patch\":{\"id\":1},\"timeMs\":0.0}],"
      "\"requestTimeMs\":0.0}",
      fetcher1->upload_data());
  fetcher1->set_response_code(200);
  fetcher1->delegate()->OnURLFetchComplete(fetcher1);
  EXPECT_EQ(GcdRestClient::SUCCESS, last_status_);
  EXPECT_EQ(2, counter_);
}

TEST_F(GcdRestClientTest, SucceedAndDelete) {
  CreateClient();

  client_->PatchState(MakePatchDetails(0).Pass(),
                      base::Bind(&GcdRestClientTest::OnRequestComplete,
                                 base::Unretained(this)));
  net::TestURLFetcher* fetcher0 = url_fetcher_factory_.GetFetcherByID(0);
  client_->PatchState(MakePatchDetails(1).Pass(),
                      base::Bind(&GcdRestClientTest::OnRequestComplete,
                                 base::Unretained(this)));
  delete_client_ = true;

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      "{\"patches\":[{\"patch\":{\"id\":0},\"timeMs\":0.0}],"
      "\"requestTimeMs\":0.0}",
      fetcher0->upload_data());
  fetcher0->set_response_code(200);
  fetcher0->delegate()->OnURLFetchComplete(fetcher0);
  EXPECT_EQ(GcdRestClient::SUCCESS, last_status_);
  EXPECT_EQ(1, counter_);

  base::RunLoop().RunUntilIdle();
}

}  // namespace remoting
