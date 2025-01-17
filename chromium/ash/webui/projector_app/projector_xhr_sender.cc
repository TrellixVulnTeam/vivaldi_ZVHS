// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/projector_xhr_sender.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "base/bind.h"
#include "base/strings/string_util.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Projector network traffic annotation tags.
constexpr net::NetworkTrafficAnnotationTag kNetworkTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("projector_xhr_loader", R"(
          semantics: {
            sender: "ChromeOS Projector"
            description: "ChromeOS send Projector XHR requests"
            destination: GOOGLE_OWNED_SERVICE
          }
          policy: {
            cookies_allowed: YES
          })");

constexpr char kAuthorizationHeaderPrefix[] = "Bearer ";

constexpr char kTranslationApiDomain[] = "translation.googleapis.com";

constexpr char kApiKeyParam[] = "key";

// List of URL prefix supported by `ProjectorXhrSender`.
const char* kUrlAllowlist[] = {
    "https://www.googleapis.com/drive/v3/files/",
    "https://www.googleapis.com/upload/drive/v3/files/",
    // TODO(b/229792620): Remove this URL prefix once web component is updated
    // with the base URL that force using primary account credential.
    "https://drive.google.com/get_video_info",
    "https://drive.google.com/u/0/get_video_info",
    "https://translation.googleapis.com/language/translate/v2"};

// Return true if the url matches the allowed URL prefix.
bool IsUrlAllowlisted(const std::string& url) {
  for (auto* urlPrefix : kUrlAllowlist) {
    if (base::StartsWith(url, urlPrefix, base::CompareCase::SENSITIVE))
      return true;
  }
  return false;
}

// The maximum number of retries for the SimpleURLLoader requests. Three times
// is an arbitrary number to start with.
const int kMaxRetries = 3;

}  // namespace

ProjectorXhrSender::ProjectorXhrSender(
    network::mojom::URLLoaderFactory* url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}
ProjectorXhrSender::~ProjectorXhrSender() = default;

void ProjectorXhrSender::Send(const GURL& url,
                              const std::string& method,
                              const std::string& request_body,
                              bool use_credentials,
                              SendRequestCallback callback,
                              const base::Value::Dict& headers) {
  if (!IsUrlAllowlisted(url.spec())) {
    std::move(callback).Run(
        /*success=*/false,
        /*response_body=*/std::string(),
        /*error=*/"UNSUPPORTED_URL");
    return;
  }

  // TODO(b/244190982): pass an option to use api key from the XhrSender api
  // instead.
  bool use_api_key =
      ash::features::IsProjectorUseApiKeyForTranslationEnabled() &&
      url.DomainIs(kTranslationApiDomain);
  GURL request_url = url;
  if (use_api_key) {
    request_url =
        net::AppendQueryParameter(url, kApiKeyParam, google_apis::GetAPIKey());
  }

  if (use_credentials || use_api_key) {
    // Use end user credentials or API key to authorize the request. Doesn't
    // need to fetch OAuth token.
    SendRequest(request_url, method, request_body, /*token=*/std::string(),
                headers, std::move(callback));
    return;
  }

  // Fetch OAuth token for authorizing the request.
  // TODO(b/197366265): add support for secondary account.
  auto primary_account =
      ProjectorAppClient::Get()->GetIdentityManager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
  oauth_token_fetcher_.GetAccessTokenFor(
      primary_account.email,
      base::BindOnce(&ProjectorXhrSender::OnAccessTokenRequestCompleted,
                     weak_factory_.GetWeakPtr(), request_url, method,
                     request_body, headers.Clone(), std::move(callback)));
}

void ProjectorXhrSender::OnAccessTokenRequestCompleted(
    const GURL& url,
    const std::string& method,
    const std::string& request_body,
    const base::Value::Dict& headers,
    SendRequestCallback callback,
    const std::string& email,
    GoogleServiceAuthError error,
    const signin::AccessTokenInfo& info) {
  if (error.state() != GoogleServiceAuthError::State::NONE) {
    std::move(callback).Run(
        /*success=*/false,
        /*response_body=*/std::string(),
        /*error=*/"TOKEN_FETCH_FAILURE");
    return;
  }

  SendRequest(url, method, request_body, info.token, headers,
              std::move(callback));
}

void ProjectorXhrSender::SendRequest(const GURL& url,
                                     const std::string& method,
                                     const std::string& request_body,
                                     const std::string& token,
                                     const base::Value::Dict& headers,
                                     SendRequestCallback callback) {
  // Build resource request.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = method;
  // The OAuth token will be empty if the request is using end user credentials
  // for authorization.
  if (!token.empty()) {
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                        kAuthorizationHeaderPrefix + token);
  }
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      "application/json");

  for (auto [key, value] : headers) {
    resource_request->headers.SetHeader(key, value.GetString());
  }

  // Send resource request.
  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kNetworkTrafficAnnotationTag);

  if (!request_body.empty())
    loader->AttachStringForUpload(request_body, "application/json");
  loader->SetRetryOptions(
      kMaxRetries,
      network::SimpleURLLoader::RETRY_ON_5XX |
          network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_,
      base::BindOnce(&ProjectorXhrSender::OnSimpleURLLoaderComplete,
                     weak_factory_.GetWeakPtr(), next_request_id_,
                     std::move(callback)));

  loader_map_.emplace(next_request_id_++, std::move(loader));
}

void ProjectorXhrSender::OnSimpleURLLoaderComplete(
    int request_id,
    SendRequestCallback callback,
    std::unique_ptr<std::string> response_body) {
  auto& loader = loader_map_[request_id];
  if (!response_body || loader->NetError() != net::OK ||
      !loader->ResponseInfo() || !loader->ResponseInfo()->headers) {
    std::move(callback).Run(
        /*success=*/false,
        /*response_body=*/std::string(),
        /*error=*/"XHR_FETCH_FAILURE");
  } else {
    std::move(callback).Run(
        /*success=*/true,
        /*response_body=*/*response_body,
        /*error=*/std::string());
  }

  loader_map_.erase(request_id);
}

}  // namespace ash
