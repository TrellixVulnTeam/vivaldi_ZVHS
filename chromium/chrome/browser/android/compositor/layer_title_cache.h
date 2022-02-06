// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TITLE_CACHE_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TITLE_CACHE_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/bind.h"
#include "base/id_map.h"
#include "cc/resources/ui_resource_client.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

namespace cc {
class Layer;
class UIResourceLayer;
}

namespace ui {
class ResourceManager;
}

namespace chrome {
namespace android {

class DecorationTitle;

// A native component of the Java LayerTitleCache class.  This class
// will build and maintain layers that represent the cached titles in
// the Java class.
class LayerTitleCache {
 public:
  static LayerTitleCache* FromJavaObject(jobject jobj);

  LayerTitleCache(JNIEnv* env,
                  jobject jobj,
                  jint fade_width,
                  jint favicon_start_padding,
                  jint favicon_end_padding,
                  jint spinner_resource_id,
                  jint spinner_incognito_resource_id);
  void Destroy(JNIEnv* env, jobject obj);

  // Called from Java, updates a native cc::Layer based on the new texture
  // information.
  void UpdateLayer(JNIEnv* env,
                   jobject obj,
                   jint tab_id,
                   jint title_resource_id,
                   jint favicon_resource_id,
                   bool is_incognito,
                   bool is_rtl);

  void ClearExcept(JNIEnv* env, jobject obj, jint except_id);

  // Returns the layer that represents the title of tab of tab_id.
  // Returns NULL if no layer can be found.
  DecorationTitle* GetTitleLayer(int tab_id);

  void SetResourceManager(ui::ResourceManager* resource_manager);

 private:
  virtual ~LayerTitleCache();

  IDMap<DecorationTitle, IDMapOwnPointer> layer_cache_;

  JavaObjectWeakGlobalRef weak_java_title_cache_;
  int fade_width_;
  int favicon_start_padding_;
  int favicon_end_padding_;

  int spinner_resource_id_;
  int spinner_incognito_resource_id_;

  ui::ResourceManager* resource_manager_;

  DISALLOW_COPY_AND_ASSIGN(LayerTitleCache);
};

bool RegisterLayerTitleCache(JNIEnv* env);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TITLE_CACHE_H_
