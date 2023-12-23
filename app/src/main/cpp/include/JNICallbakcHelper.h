#ifndef DERRYPLAYER_JNICALLBAKCHELPER_H
#define DERRYPLAYER_JNICALLBAKCHELPER_H

#include <jni.h>
#include "util.h"

class JNICallbakcHelper {

private:
    JavaVM *vm = 0;   // 只有他 才能 跨越线程
    JNIEnv *env = 0; // 主线程 调用 Kotlin 用的
    jobject job; // 为了更好的寻找到 BKPlayer.kt实例
    jmethodID jmd_prepared;
    jmethodID jmd_onError;
    jmethodID jmd_onProgress; // 播放音频的时间搓回调

public:
    JNICallbakcHelper(JavaVM *vm, JNIEnv *env, jobject job);
    ~JNICallbakcHelper();

    void onPrepared(int thread_mode);
    void onError(int thread_mode, int error_code, char * ffmpegError);

    void onProgress(int thread_mode, int audio_time);

};

#endif //DERRYPLAYER_JNICALLBAKCHELPER_H
