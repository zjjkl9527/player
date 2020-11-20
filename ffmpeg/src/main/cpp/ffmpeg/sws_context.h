#ifndef FFMPEG_SWS_CONTEXT_H
#define FFMPEG_SWS_CONTEXT_H

#include "util.h"

extern "C" {
#include "libswscale/swscale.h"
}

namespace ffmpeg {

class SWSContext {
 public:
  SWSContext(int srcW, int srcH, AVPixelFormat srcFmt, int dstW, int dstH,
             AVPixelFormat dstFmt, int flag = SWS_FAST_BILINEAR) {
    auto p = sws_getContext(srcW, srcH, srcFmt, dstW, dstH, dstFmt, flag,
                            nullptr, nullptr, nullptr);
    if (p == nullptr)
      throw std::runtime_error("sws_getContext() error");
    else
      mSwsCtx.reset(p);
  }

  int scale(const uint8_t* const src[], const int srcStride[], int srcSliceY,
            int srcSliceH, uint8_t* const dst[],
            const int dstStride[]) noexcept {
    return sws_scale(mSwsCtx.get(), src, srcStride, srcSliceY, srcSliceH, dst,
                     dstStride);
  }

  SwsContext* get() noexcept { return mSwsCtx.get(); }

  const SwsContext* get() const noexcept { return mSwsCtx.get(); }

  SwsContext* operator->() noexcept { return mSwsCtx.get(); }

  const SwsContext* operator->() const noexcept { return mSwsCtx.get(); }

 private:
  struct Deletor {
    void operator()(SwsContext* p) {
      if (p == nullptr) return;
      sws_freeContext(p);
    }
  };

  std::unique_ptr<SwsContext, Deletor> mSwsCtx{nullptr, Deletor()};
};

};  // namespace ffmpeg

#endif  // SWS_CONTEXT_H
