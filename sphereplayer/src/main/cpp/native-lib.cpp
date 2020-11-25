#include <jni.h>
#include <string>
#include "ffmpeg/demuxer.h"
#include <android/log.h>

#define TAG "FFMPEG"
#define LOG_DEBUG(...) __android_log_print(ANDROID_LOG_DEBUG,TAG ,__VA_ARGS__)
#define LOG_INFO(...) __android_log_print(ANDROID_LOG_INFO,TAG ,__VA_ARGS__)
#define LOG_WARN(...) __android_log_print(ANDROID_LOG_WARN,TAG ,__VA_ARGS__)
#define LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR,TAG , __VA_ARGS__)
#define LOG_FATAL(...) __android_log_print(ANDROID_LOG_FATAL,TAG ,__VA_ARGS__)

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_ffmpeg_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";

    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_hhqj_player_SpherePlayer_00024Demuxer_createDemuxer(JNIEnv *, jobject ) noexcept {
    return (jlong)(new ffmpeg::Demuxer());
}

extern "C" JNIEXPORT void JNICALL
Java_com_hhqj_player_SpherePlayer_00024Demuxer_destroyDemuxer(JNIEnv *, jobject , jlong pointer) noexcept {
    const auto demuxer = (ffmpeg::Demuxer*)pointer;
    if(demuxer) delete demuxer;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_hhqj_player_SpherePlayer_00024Demuxer_openInput(JNIEnv *env, jobject obj, jlong pointer, jstring url, jobject callback) noexcept {
    auto urlString = env->GetStringUTFChars(url, 0);
    const auto demuxer = (ffmpeg::Demuxer*)pointer;

    auto ret = demuxer->openInput(urlString);
    if(ret < 0) return ret;

    env->ReleaseStringUTFChars(url, urlString);

    jobject refCallback = env->NewGlobalRef(callback);

    ret = demuxer->openVideoOutput([=](uint8_t* data, int len){
        auto jclass = env->GetObjectClass(refCallback);
        auto jmethod = env->GetMethodID(jclass, "onVideo", "([BJ)V");

        jbyteArray byteArray = env->NewByteArray(len);
        env->SetByteArrayRegion(byteArray, 0, len, (jbyte *) data);
        env->CallVoidMethod(refCallback, jmethod, byteArray, demuxer->videoPTS());
        return len;
    });
    if(ret < 0) return ret;



    ret = demuxer->openAudioOutput([=](uint8_t* data, int len){
        auto jclass = env->GetObjectClass(refCallback);
        auto jmethod = env->GetMethodID(jclass, "onAudio", "([BJ)V");

        jbyteArray byteArray = env->NewByteArray(len);
        env->SetByteArrayRegion(byteArray, 0, len, (jbyte *) data);
        env->CallVoidMethod(refCallback, jmethod, byteArray, demuxer->audioPTS());
        return len;
    });

    return ret;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_hhqj_player_SpherePlayer_00024Demuxer_flush(JNIEnv *, jobject , jlong pointer) noexcept {
    const auto demuxer = (ffmpeg::Demuxer*)pointer;
    return demuxer->flush();
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_hhqj_player_SpherePlayer_00024Demuxer_getErrorString(JNIEnv *env, jobject , jint value) noexcept {
    auto str = ffmpeg::getErrorString(value);
    std::vector<jchar> charBuf(str.size());
    for(uint32_t i=0; i<str.size(); ++i){
        charBuf[i] = str[i];
    }
    return env->NewString(charBuf.data(), charBuf.size());
}
