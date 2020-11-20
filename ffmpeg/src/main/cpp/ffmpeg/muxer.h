#ifndef FFMPEG_MUXER_H
#define FFMPEG_MUXER_H

#include "format_context.h"
#include "packet.h"

namespace ffmpeg {

class Muxer {
   public:
    Muxer() noexcept {}

    template <typename T>
    int openVideoInput(T &&t, std::string fmt = "") noexcept {
        int ret = mVideoInput.openInput(std::forward<T>(t), fmt);
        if (ret < 0) return ret;
        ret = mVideoInput.findStreamInfo();
        return ret;
    }

    template <typename T>
    int openAudioInput(T &&t, std::string fmt = "") noexcept {
        int ret = mAudioInput.openInput(std::forward<T>(t), fmt);
        if (ret < 0) return ret;
        ret = mAudioInput.findStreamInfo();
        return ret;
    }

    void setVideoFrameRate(int frameRate) noexcept {
        if (mInputVideoStream) {
            mInputVideoStream->r_frame_rate = {frameRate, 1};
            mInputVideoStream->avg_frame_rate = {frameRate, 1};
        }
    }

    FormatContext &videoInput() noexcept { return mVideoInput; }

    FormatContext &audioInput() noexcept { return mAudioInput; }

    int openOutput(std::string url, std::string fmt = "") noexcept {
        int ret = mOutput.openOutput(url, fmt);
        if (ret < 0) return ret;

        return addOutputStreams();
    }

    int openOutput(std::function<int(uint8_t *, int)> cb,
                   std::string fmt) noexcept {
        int ret = mOutput.openOutput(cb, fmt);
        if (ret < 0) return ret;

        return addOutputStreams();
    }

    FormatContext &output() noexcept { return mOutput; }

    int flushVideo() noexcept {
        Packet packet;
        int ret = mVideoInput.read(packet);
        if (ret < 0) return ret;

        double frameRate = av_q2d(mInputVideoStream->r_frame_rate);

        if (mAudioInput.isOpened()) {
            if (mVideoPTS > mAudioPTS)
                frameRate *= 1.05;
            else if (mVideoPTS < mAudioPTS)
                frameRate *= 0.95;
        }

        packet->duration = std::round(
            1.0 / (frameRate * av_q2d(mInputVideoStream->time_base)));

        av_packet_rescale_ts(packet.get(), mInputVideoStream->time_base,
                             mOutputVideoStream->time_base);
        mVideoPTS += packet->duration;
        packet->pts = mVideoPTS;
        packet->dts = mVideoPTS;
        packet->stream_index = mOutputVideoStream->index;
        return mOutput.write(packet);
    }

    int flushAudio() noexcept {
        Packet packet;
        int ret = mAudioInput.read(packet);
        if (ret < 0) return ret;

        av_packet_rescale_ts(packet.get(), mInputAudioStream->time_base,
                             mOutputAudioStream->time_base);
        mAudioPTS += packet->duration;
        packet->pts = mAudioPTS;
        packet->dts = mAudioPTS;
        packet->stream_index = mOutputAudioStream->index;
        return mOutput.write(packet);
    }

    int flush() noexcept {
        if (mVideoInput.isOpened()) {
            int ret = flushVideo();
            if (ret < 0 && ret != AVERROR(EAGAIN)) return ret;
        }

        if (mAudioInput.isOpened()) {
            int ret = flushAudio();
            if (ret < 0 && ret != AVERROR(EAGAIN)) return ret;
        }

        return 0;
    }

    int close() { return mOutput.writeTrailer(); }

   private:
    FormatContext mVideoInput;
    FormatContext mAudioInput;
    FormatContext mOutput;
    AVStream *mInputVideoStream = nullptr;
    AVStream *mInputAudioStream = nullptr;
    AVStream *mOutputVideoStream = nullptr;
    AVStream *mOutputAudioStream = nullptr;
    int64_t mVideoPTS = 0;
    int64_t mAudioPTS = 0;

    int addOutputStreams() noexcept {
        if (mVideoInput.isOpened() && mVideoInput->nb_streams > 0) {
            auto index = mVideoInput.findBestStream(AVMEDIA_TYPE_VIDEO);
            if (index >= 0) {
                mOutputVideoStream = mOutput.newStream();
                assert(mOutputVideoStream != nullptr);
                mInputVideoStream = mVideoInput->streams[index];
                int ret = avcodec_parameters_copy(mOutputVideoStream->codecpar,
                                                  mInputVideoStream->codecpar);
                if (ret < 0) return ret;
            }
        }

        if (mAudioInput.isOpened()) {
            auto index = mAudioInput.findBestStream(AVMEDIA_TYPE_AUDIO);
            if (index >= 0) {
                mOutputAudioStream = mOutput.newStream();
                assert(mOutputAudioStream != nullptr);
                mInputAudioStream = mAudioInput->streams[index];
                int ret = avcodec_parameters_copy(mOutputAudioStream->codecpar,
                                                  mInputAudioStream->codecpar);
                if (ret < 0) return ret;
            }
        }

        return mOutput.writeHeader();
    }
};
}  // namespace ffmpeg

#endif  // MUX_H
