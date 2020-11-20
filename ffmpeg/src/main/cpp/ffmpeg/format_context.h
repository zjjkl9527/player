#ifndef FFMPEG_FORMAT_CONTEXT_H
#define FFMPEG_FORMAT_CONTEXT_H

#include "frame.h"
#include "packet.h"
#include "util.h"

extern "C" {
#include "libavformat/avformat.h"
}

namespace ffmpeg {

class FormatContext {
   public:
    FormatContext() noexcept {}

    FormatContext(FormatContext&&) = default;

    FormatContext& operator=(FormatContext&&) = default;

    virtual ~FormatContext() = default;

    AVFormatContext* operator->() noexcept { return mFmtCtx.get(); }

    const AVFormatContext* operator->() const noexcept { return mFmtCtx.get(); }

    AVFormatContext* get() noexcept { return mFmtCtx.get(); }

    const AVFormatContext* get() const noexcept { return mFmtCtx.get(); }

    bool isOpened() { return mFmtCtx != nullptr; }

    int openInput(std::function<int(uint8_t*, int)> readCb,
                  std::string fmt = "") noexcept {
        mCallback = readCb;

        auto ctx = avformat_alloc_context();
        auto ioContex = avio_alloc_context(
            (uint8_t*)av_malloc(AVIO_BUF_SIZE), AVIO_BUF_SIZE, 0, this,
            &FormatContext::callback, nullptr, nullptr);
        ctx->pb = ioContex;

        int ret = 0;
        if (fmt.size() > 0) {
            ret = avformat_open_input(
                &ctx, nullptr, av_find_input_format(fmt.c_str()), nullptr);
        } else {
            ret = avformat_open_input(&ctx, nullptr, nullptr, nullptr);
        }
        mFmtCtx.reset(ctx);

        return ret;
    }

    int openInput(std::string url, std::string fmt = "") noexcept {
        auto ctx = avformat_alloc_context();

        int ret = 0;
        if (fmt.size() > 0)
            ret = avformat_open_input(
                &ctx, nullptr, av_find_input_format(fmt.c_str()), nullptr);
        else
            ret = avformat_open_input(&ctx, url.c_str(), nullptr, nullptr);
        mFmtCtx.reset(ctx);

        return ret;
    }

    int openOutput(std::function<int(uint8_t*, int32_t)> writeCb,
                   std::string fmt) noexcept {
        mCallback = writeCb;

        auto ctx = avformat_alloc_context();
        auto ioContext = avio_alloc_context((uint8_t*)av_malloc(AVIO_BUF_SIZE),
                                            AVIO_BUF_SIZE, 1, this, nullptr,
                                            &FormatContext::callback, nullptr);

        auto ret =
            avformat_alloc_output_context2(&ctx, nullptr, fmt.c_str(), nullptr);

        ctx->pb = ioContext;

        mFmtCtx.reset(ctx);

        return ret;
    }

    int openOutput(std::string url, std::string fmt = "") noexcept {
        auto ctx = avformat_alloc_context();

        int ret = 0;
        if (fmt.size() > 0) {
            ret = avformat_alloc_output_context2(&ctx, nullptr, fmt.c_str(),
                                                 url.c_str());
        } else {
            ret = avformat_alloc_output_context2(&ctx, nullptr, nullptr,
                                                 url.c_str());
        }

        mFmtCtx.reset(ctx);
        if (ret < 0) return ret;

        ret = avio_open(&mFmtCtx->pb, url.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) return ret;

        return 0;
    }

    AVStream* newStream(const AVCodec* codec = nullptr) noexcept {
        return avformat_new_stream(mFmtCtx.get(), codec);
    }

    int findBestStream(AVMediaType type, int wantedNum = -1, int related = -1,
                       AVCodec** decoder = nullptr, int flags = 0) noexcept {
        return av_find_best_stream(mFmtCtx.get(), type, wantedNum, related,
                                   decoder, flags);
    }

    int findStreamInfo(AVDictionary** options = nullptr) noexcept {
        return avformat_find_stream_info(mFmtCtx.get(), options);
    }

    void dumpFormat(const char* url, bool isOutput) noexcept {
        av_dump_format(mFmtCtx.get(), 0, url, isOutput);
    }

    int readFrame(AVPacket* packet) noexcept {
        return av_read_frame(mFmtCtx.get(), packet);
    }

    int read(Packet& packet) noexcept {
        return av_read_frame(mFmtCtx.get(), packet.get());
    }

    int writeFrame(AVPacket* packet) noexcept {
        return av_interleaved_write_frame(mFmtCtx.get(), packet);
    }

    int write(Packet& packet) noexcept {
        return av_interleaved_write_frame(mFmtCtx.get(), packet.get());
    }

    int writeHeader(AVDictionary** options = nullptr) noexcept {
        return avformat_write_header(mFmtCtx.get(), options);
    }

    int writeTrailer() { return av_write_trailer(mFmtCtx.get()); }

   protected:
    static constexpr size_t AVIO_BUF_SIZE = 4 * 1024 * 1024;

    struct Deletor {
        void operator()(AVFormatContext* p) {
            if (p) {
                avformat_free_context(p);
            }
        }
    };
    std::unique_ptr<AVFormatContext, Deletor> mFmtCtx{nullptr, Deletor()};
    std::function<int(uint8_t*, int)> mCallback;

    static int callback(void* opaque, uint8_t* buf, int32_t size) noexcept {
        auto p = static_cast<FormatContext*>(opaque);
        return p->mCallback(buf, size);
    }
};

};  // namespace ffmpeg

#endif  // FORMAT_CONTEXT_H
