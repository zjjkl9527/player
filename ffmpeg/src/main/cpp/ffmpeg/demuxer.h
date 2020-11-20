/*
 * @Author: zck
 * @Date: 2020-11-06 11:38:48
 * @LastEditTime: 2020-11-06 15:01:12
 * @Description: file content
 */
#ifndef FFMPEG_DEMUXER_H
#define FFMPEG_DEMUXER_H

#include "format_context.h"
#include "packet.h"
#include <cmath>

namespace ffmpeg {

class Demuxer {
   public:
    template <typename T>
    int openInput(T&& t, std::string fmt = "") noexcept {
        int ret = mInput.openInput(std::forward<T>(t), fmt);
        if (ret < 0) return ret;

        return mInput.findStreamInfo();
    }

    template <typename T>
    int openVideoOutput(T&& t) noexcept {
        mVideoIndex = mInput.findBestStream(AVMEDIA_TYPE_VIDEO);
        if (mVideoIndex < 0) return mVideoIndex;

        auto codecName =
            avcodec_get_name(mInput->streams[mVideoIndex]->codecpar->codec_id);

        int ret = mVideoOutput.openOutput(std::forward<T>(t), codecName);
        if (ret < 0) return ret;

        auto stream = mVideoOutput.newStream();
        assert(stream != nullptr);
        ret = avcodec_parameters_copy(stream->codecpar,
                                      mInput->streams[mVideoIndex]->codecpar);
        if (ret < 0) return ret;

        return mVideoOutput.writeHeader();
    }

    template <typename T>
    int openAudioOutput(T&& t) noexcept {
        mAudioIndex = mInput.findBestStream(AVMEDIA_TYPE_AUDIO);
        if (mAudioIndex < 0) return mAudioIndex;

        int ret = mAudioOutput.openOutput(std::forward<T>(t), "adts");
        if (ret < 0) return ret;

        auto stream = mAudioOutput.newStream();
        assert(stream != nullptr);
        ret = avcodec_parameters_copy(stream->codecpar,
                                      mInput->streams[mAudioIndex]->codecpar);
        if (ret < 0) return ret;

        return mAudioOutput.writeHeader();
    }

    FormatContext& input() noexcept { return mInput; }

    FormatContext& videoOutput() noexcept { return mVideoOutput; }

    FormatContext& audioOutput() noexcept { return mAudioOutput; }

    int64_t videoPTS() const noexcept { return mVideoPTS; }

    int64_t audioPTS() const noexcept { return mAudioPTS; }

    int flush() {
        Packet packet;
        int ret = mInput.read(packet);
        if (ret < 0) return ret;

        if (packet->stream_index == mVideoIndex && mVideoOutput.isOpened()) {
            packet->stream_index = 0;
            mVideoPTS = std::round(
                packet->pts * av_q2d(mInput->streams[mVideoIndex]->time_base) *
                1000.0);
            return mVideoOutput.write(packet);
        } else if (packet->stream_index == mAudioIndex &&
                   mAudioOutput.isOpened()) {
            packet->stream_index = 0;
            mAudioPTS = std::round(
                packet->pts * av_q2d(mInput->streams[mAudioIndex]->time_base) *
                1000.0);
            return mAudioOutput.write(packet);
        }

        return 0;
    }

    int close() {
        if (mVideoOutput.isOpened()) {
            int ret = mVideoOutput.writeTrailer();
            if (ret < 0) return ret;
        }

        if (mAudioOutput.isOpened()) {
            int ret = mAudioOutput.writeTrailer();
            if (ret < 0) return ret;
        }
        return 0;
    }

   private:
    FormatContext mInput;
    FormatContext mVideoOutput;
    FormatContext mAudioOutput;
    int mVideoIndex = 0;
    int mAudioIndex = 0;
    int64_t mVideoPTS = -1;
    int64_t mAudioPTS = -1;
};

}  // namespace ffmpeg

#endif