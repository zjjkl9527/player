/*
 * @Author: zck
 * @Date: 2020-11-03 09:37:02
 * @LastEditTime: 2020-11-06 09:57:11
 * @Description: file content
 */
#ifndef FFMPEG_UTIL_H
#define FFMPEG_UTIL_H

#include <atomic>
#include <cassert>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include "libavutil/avutil.h"
}

namespace ffmpeg {

// void showHardwareCoder() {
//  AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
//  while (true) {
//    type = av_hwdevice_iterate_types(type);
//    if (type == AV_HWDEVICE_TYPE_NONE)
//      break;
//    else {
//      LOG_INFO("Find hardware device {}", av_hwdevice_get_type_name(type));
//    }
//  }
//}

inline std::string getErrorString(int error) noexcept {
    char buf[64];
    av_make_error_string(buf, sizeof(buf), error);
    return std::string(buf);
}

class JoinThread {
   public:
    class StopFlag {
       public:
        void requestStop() noexcept {
            mStopRequst.store(true, std::memory_order_release);
        }

        bool stopRequested() noexcept {
            return mStopRequst.load(std::memory_order_acquire);
        }

       private:
        std::atomic<bool> mStopRequst{false};
    };

    template <class Fn, class... Args>
    JoinThread(Fn&& func, Args&&... args) noexcept
        : mThread([](StopFlag& stopFlag, Fn&& func,
                     Args&&... args) { func(stopFlag, args...); },
                  std::ref(mStopFlag), std::forward<Fn>(func),
                  std::forward<Args>(args)...) {}

    virtual ~JoinThread() noexcept {
        mStopFlag.requestStop();
        mThread.join();
    }

    void requestStop() noexcept { mStopFlag.requestStop(); }

    bool joinable() noexcept { return mThread.joinable(); }

    void join() noexcept { mThread.join(); }

   private:
    std::thread mThread;
    StopFlag mStopFlag;
};

};  // namespace ffmpeg

#endif  // UTIL_H
