#ifndef FFMPEG_CODEC_CONTEXT_H
#define FFMPEG_CODEC_CONTEXT_H

#include "util.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/hwcontext.h"
#include "libavutil/pixdesc.h"
}

namespace ffmpeg {

class CodecContext {
   public:
    CodecContext() noexcept {}

    CodecContext(AVCodec* codec) {
        mCodecCtx.reset(avcodec_alloc_context3(codec));

        if (mCodecCtx == nullptr) {
            throw std::runtime_error("avcodec_alloc_context3() error");
        }
    }

    AVCodecContext* operator->() noexcept { return mCodecCtx.get(); }

    const AVCodecContext* operator->() const noexcept {
        return mCodecCtx.get();
    }

    AVCodecContext* get() noexcept { return mCodecCtx.get(); }

    const AVCodecContext* get() const noexcept { return mCodecCtx.get(); }

    int setParameter(const AVCodecParameters* para) noexcept {
        return avcodec_parameters_to_context(mCodecCtx.get(), para);
    }

    int open(const AVCodec* codec) noexcept {
        return avcodec_open2(mCodecCtx.get(), codec, nullptr);
    }

    int createHardwareContext(AVHWDeviceType type) noexcept {
        return av_hwdevice_ctx_create(&mCodecCtx->hw_device_ctx, type, nullptr,
                                      nullptr, 0);
    }

    int sendPacket(AVPacket* packet) noexcept {
        return avcodec_send_packet(mCodecCtx.get(), packet);
    }

    int receiveFrame(AVFrame* frame) noexcept {
        return avcodec_receive_frame(mCodecCtx.get(), frame);
    }

    AVCodecContext* ptr() { return mCodecCtx.get(); }

   private:
    struct Deletor {
        void operator()(AVCodecContext* p) {
            if (p == nullptr) return;
            avcodec_free_context(&p);
        }
    };

    std::unique_ptr<AVCodecContext, Deletor> mCodecCtx{
        avcodec_alloc_context3(nullptr), Deletor()};
};

}  // namespace ffmpeg

#endif  // CODEC_CONTEXT_H
