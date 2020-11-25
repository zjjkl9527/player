/*
 * @Author: zck
 * @Date: 2020-11-03 09:37:02
 * @LastEditTime: 2020-11-04 09:20:06
 * @Description: file conten
 */
#ifndef FFMPEG_FRAME_H
#define FFMPEG_FRAME_H

#include "util.h"

extern "C" {
#include "libavutil/frame.h"
}

namespace ffmpeg {

class Frame {
   public:
    Frame() {}

    Frame(AVPixelFormat fmt, int width, int height) {
        mFrame->format = fmt;
        mFrame->width = width;
        mFrame->height = height;

        auto ret = av_frame_get_buffer(mFrame.get(), 0);
        if (ret < 0) {
            char buf[64];
            throw std::runtime_error(
                av_make_error_string(buf, sizeof(buf), ret));
        }
    }

    AVFrame* get() noexcept { return mFrame.get(); }

    const AVFrame* get() const noexcept { return mFrame.get(); }

    AVFrame* operator->() noexcept { return mFrame.get(); }

    const AVFrame* operator->() const noexcept { return mFrame.get(); }

   private:
    struct Deletor {
        void operator()(AVFrame* p) {
            if (p) {
                // av_frame_unref(p);
                av_frame_free(&p);
            }
        }
    };

    std::unique_ptr<AVFrame, Deletor> mFrame{av_frame_alloc(), Deletor()};
};

};  // namespace ffmpeg

#endif  // FRAME_H
