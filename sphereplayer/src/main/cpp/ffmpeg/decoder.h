#ifndef FFMPEG_DECODER_H
#define FFMPEG_DECODER_H

#include <chrono>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

#include "codec_context.h"
#include "format_context.h"
#include "frame.h"
#include "packet.h"

namespace ffmpeg {

class Decoder {
   public:
    Decoder() noexcept {}

    int init(const std::function<int(uint8_t*, int)>& readCb,
             uint32_t bufSize = 4 * 1024 * 1024,
             AVHWDeviceType hardware = AV_HWDEVICE_TYPE_NONE,
             std::string fmt = "") {
        auto ret = mInputFmtCtx.openInput(readCb, fmt);
        if (ret < 0) return ret;

        AVCodec* codec = nullptr;
        auto index =
            mInputFmtCtx.findBestStream(AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        if (index < 0 || codec == nullptr) return index;
        mVideoStreamIndex = index;

        if (hardware == AV_HWDEVICE_TYPE_NONE) {
            return initCodecContext(codec);
        } else {
            return initCodecContext(codec, hardware);
        }
    }

    int init(const std::function<int(uint8_t*, int)>& readCb,
             uint32_t bufSize = 4 * 1024 * 1024, bool useHWDeocder = true,
             std::string fmt = "") {
        auto ret = mInputFmtCtx.openInput(readCb, fmt);
        if (ret < 0) return ret;

        AVCodec* codec = nullptr;
        auto index =
            mInputFmtCtx.findBestStream(AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        if (index < 0 || codec == nullptr) return index;
        mVideoStreamIndex = index;

        if (useHWDeocder) {
            int ret = 0;

            ret = initCodecContext(codec, AV_HWDEVICE_TYPE_D3D11VA);
            if (ret >= 0) return ret;

            ret = initCodecContext(codec, AV_HWDEVICE_TYPE_DXVA2);
            if (ret >= 0) return ret;

            ret = initCodecContext(codec, AV_HWDEVICE_TYPE_CUDA);
            if (ret >= 0) return ret;
        }
        return initCodecContext(codec);
    }

    int init(const std::string& url,
             AVHWDeviceType hardware = AV_HWDEVICE_TYPE_NONE,
             std::string fmt = "") {
        auto ret = mInputFmtCtx.openInput(url, fmt);
        if (ret < 0) return ret;

        AVCodec* codec = nullptr;
        auto index =
            mInputFmtCtx.findBestStream(AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        if (index < 0 || codec == nullptr) return index;
        mVideoStreamIndex = index;

        if (hardware == AV_HWDEVICE_TYPE_NONE) {
            return initCodecContext(codec);
        } else {
            return initCodecContext(codec, hardware);
        }
    }

    int init(const std::string& url, bool useHWDeocder = true,
             std::string fmt = "") {
        auto ret = mInputFmtCtx.openInput(url, fmt);
        if (ret < 0) return ret;

        AVCodec* codec = nullptr;
        auto index =
            mInputFmtCtx.findBestStream(AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        if (index < 0 || codec == nullptr) return index;
        mVideoStreamIndex = index;

        if (useHWDeocder) {
            int ret = 0;

            ret = initCodecContext(codec, AV_HWDEVICE_TYPE_D3D11VA);
            if (ret >= 0) return ret;

            ret = initCodecContext(codec, AV_HWDEVICE_TYPE_DXVA2);
            if (ret >= 0) return ret;

            ret = initCodecContext(codec, AV_HWDEVICE_TYPE_CUDA);
            if (ret >= 0) return ret;
        }
        return initCodecContext(codec);
    }

    int decode(Frame& frame) {
        auto ret = mCodecCtx.receiveFrame(frame.get());
        if (ret == 0)
            return 0;
        else if (ret != AVERROR(EAGAIN))
            return ret;

        Packet packet;
        while (true) {
            ret = mInputFmtCtx.readFrame(packet.get());
            if (ret < 0) return ret;
            if (packet->stream_index == mVideoStreamIndex) break;
        }

        ret = mCodecCtx.sendPacket(packet.get());
        if (ret < 0) return ret;

        ret = mCodecCtx.receiveFrame(frame.get());
        return ret;
    }

    double frameRate() const noexcept {
        if (mVideoStreamIndex < 0) return -1;

        return (double)mInputFmtCtx->streams[mVideoStreamIndex]
                   ->avg_frame_rate.num /
               (double)mInputFmtCtx->streams[mVideoStreamIndex]
                   ->avg_frame_rate.den;
    }

   private:
    FormatContext mInputFmtCtx;
    CodecContext mCodecCtx;

    int mVideoStreamIndex = -1;

    int initCodecContext(AVCodec* codec) noexcept {
        mCodecCtx = CodecContext(codec);

        auto codecpar = mInputFmtCtx->streams[mVideoStreamIndex]->codecpar;

        auto ret = mCodecCtx.setParameter(codecpar);
        if (ret < 0) return ret;

        return mCodecCtx.open(codec);
    }

    int initCodecContext(AVCodec* codec, AVHWDeviceType hw) noexcept {
        mCodecCtx = CodecContext(codec);

        auto codecpar = mInputFmtCtx->streams[mVideoStreamIndex]->codecpar;
        auto ret = mCodecCtx.setParameter(codecpar);
        if (ret < 0) return ret;

        ret = mCodecCtx.createHardwareContext(hw);
        if (ret < 0) return ret;

        return mCodecCtx.open(codec);
    }
};

};  // namespace ffmpeg

#endif
