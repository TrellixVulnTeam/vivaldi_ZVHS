// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_garbage_collector_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/extension_garbage_collector.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extensions_browser_client.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/extensions/extension_garbage_collector_chromeos.h"
#endif

namespace extensions {

// static
ExtensionGarbageCollector*
ExtensionGarbageCollectorFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionGarbageCollector*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionGarbageCollectorFactory*
ExtensionGarbageCollectorFactory::GetInstance() {
  return Singleton<ExtensionGarbageCollectorFactory>::get();
}

ExtensionGarbageCollectorFactory::ExtensionGarbageCollectorFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionGarbageCollector",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(InstallTrackerFactory::GetInstance());
}

ExtensionGarbageCollectorFactory::~ExtensionGarbageCollectorFactory() {}

// static
scoped_ptr<KeyedService> ExtensionGarbageCollectorFactory::BuildInstanceFor(
    content::BrowserContext* context) {
#if defined(OS_CHROMEOS)
  return make_scoped_ptr(new ExtensionGarbageCollectorChromeOS(context));
#else
  return make_scoped_ptr(new ExtensionGarbageCollector(context));
#endif
}

KeyedService* ExtensionGarbageCollectorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildInstanceFor(context).release();
}

bool ExtensionGarbageCollectorFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool ExtensionGarbageCollectorFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
