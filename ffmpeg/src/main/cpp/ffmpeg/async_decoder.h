#ifndef ASYNC_DECODER_H
#define ASYNC_DECODER_H

#include "decoder.h"

namespace ffmpeg {

class AsyncDecoder {
 public:
  bool start(const std::function<int(uint8_t*, int)>& readCb,
             size_t bufSize = 5 * 1024 * 1024, bool useHWDeocder = true,
             std::string fmt = "") {
    if (mThread != nullptr) return false;

    mThread.reset(new JoinThread([this, readCb, bufSize, useHWDeocder,
                                  fmt](JoinThread::StopFlag& stopFlag) {
      auto ret = this->mDecoder.init(readCb, bufSize, useHWDeocder, fmt);
      if (ret < 0) {
        onClose(stopFlag);
        return;
      }

      onInit(stopFlag);

      while (!stopFlag.stopRequested()) {
        Frame frame;
        ret = this->mDecoder.decode(frame);
        if (ret != 0 && ret != AVERROR(EAGAIN)) {
          onClose(stopFlag);
          return;
        } else {
          onDecode(stopFlag, std::move(frame));
        }
      }

      onClose(stopFlag);
    }));

    return true;
  }

  bool start(const std::string& url, bool useHWDeocder = true,
             std::string fmt = "") {
    if (mThread != nullptr) return false;

    mThread.reset(new JoinThread(
        [this, url, useHWDeocder, fmt](JoinThread::StopFlag& stopFlag) {
          auto ret = this->mDecoder.init(url, useHWDeocder, fmt);
          if (ret < 0) {
            onClose(stopFlag);
            return;
          }

          onInit(stopFlag);

          while (!stopFlag.stopRequested()) {
            Frame frame;
            ret = this->mDecoder.decode(frame);
            if (ret != 0 && ret != AVERROR(EAGAIN)) {
              onClose(stopFlag);
              return;
            } else {
              onDecode(stopFlag, std::move(frame));
            }
          }

          onClose(stopFlag);
        }));

    return true;
  }

  void stop() noexcept { mThread = nullptr; }

  double frameRate() noexcept { return mDecoder.frameRate(); }

  std::function<void(JoinThread::StopFlag&)> onInit =
      [](JoinThread::StopFlag&) {};

  std::function<void(JoinThread::StopFlag&, Frame)> onDecode =
      [](JoinThread::StopFlag&, Frame) {};

  std::function<void(JoinThread::StopFlag&)> onClose =
      [](JoinThread::StopFlag&) {};

 private:
  Decoder mDecoder;
  std::unique_ptr<JoinThread> mThread = nullptr;
};

}  // namespace ffmpeg

#endif  // ASYNC_DECODER_H
