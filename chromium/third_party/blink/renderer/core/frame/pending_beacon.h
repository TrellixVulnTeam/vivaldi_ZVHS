// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_H_

#include "third_party/blink/public/mojom/frame/pending_beacon.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/pending_beacon_dispatcher.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class BeaconData;
class ExceptionState;
class ExecutionContext;

// Implementation of the PendingBeacon API.
// https://github.com/WICG/unload-beacon/blob/main/README.md
// Note that the lifetime of a PendingBeacon instance is not the same as the JS
// scope where the instance is created. Rather, it stays alive until
//   - roughly when `sendNow()` or `deactivate()` is called (may still be alive
//     for a while after this point).
//   - when the document where it was created is destroyed.
// See `PendingBeaconDispatcher` for more details.
class CORE_EXPORT PendingBeacon
    : public ScriptWrappable,
      public PendingBeaconDispatcher::PendingBeacon {
  DEFINE_WRAPPERTYPEINFO();

 public:
  const String& url() { return url_; }

  int32_t backgroundTimeout() const {
    return base::checked_cast<int32_t>(background_timeout_.InMilliseconds());
  }
  void setBackgroundTimeout(int32_t background_timeout);

  int32_t timeout() const {
    return base::checked_cast<int32_t>(timeout_.InMilliseconds());
  }
  void setTimeout(int32_t timeout);

  const String& method() const { return method_; }

  bool pending() const { return pending_; }

  void deactivate();

  void sendNow();

  void Trace(Visitor*) const override;

  // `PendingBeaconDispatcher::PendingBeacon` implementation.
  base::TimeDelta GetBackgroundTimeout() const override;
  void Send() override;

 protected:
  explicit PendingBeacon(ExecutionContext* context,
                         const String& url,
                         const String& method,
                         int32_t background_timeout,
                         int32_t timeout);

  void SetURLInternal(const String& url);
  void SetDataInternal(const BeaconData& beacon_data,
                       ExceptionState& exception_state);

 private:
  // A convenient method to return a TaskRunner which is able to keep working
  // even if the JS context is frozen.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();
  void TimeoutTimerFired(TimerBase*);

  Member<ExecutionContext> ec_;
  HeapMojoRemote<mojom::blink::PendingBeacon> remote_;
  String url_;
  const String method_;
  base::TimeDelta background_timeout_;
  base::TimeDelta timeout_;
  bool pending_ = true;

  // A timer to handle `setTimeout()`.
  HeapTaskRunnerTimer<PendingBeacon> timeout_timer_;
};

}  // namespace blink

#endif  // #define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_H_
