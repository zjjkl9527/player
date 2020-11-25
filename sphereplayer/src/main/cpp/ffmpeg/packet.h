/*
 * @Author: zck
 * @Date: 2020-11-03 09:37:02
 * @LastEditTime: 2020-11-04 09:19:31
 * @Description: file content
 */
#ifndef FFMPEG_PACKET_H
#define FFMPEG_PACKET_H

#include "util.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

namespace ffmpeg {

class Packet {
   public:
    Packet() noexcept {}

    AVPacket* get() noexcept { return mPacket.get(); }

    const AVPacket* get() const noexcept { return mPacket.get(); }

    AVPacket* operator->() noexcept { return mPacket.get(); }

    const AVPacket* operator->() const noexcept { return mPacket.get(); }

   private:
    struct Deletor {
        void operator()(AVPacket* p) {
            if (p) {
                // av_packet_unref(p);
                av_packet_free(&p);
            }
        }
    };

    std::unique_ptr<AVPacket, Deletor> mPacket{av_packet_alloc(), Deletor()};
};

};  // namespace ffmpeg

#endif  // PACKET_H
