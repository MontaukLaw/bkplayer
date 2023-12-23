#include <jni.h>
#include <string>
#include "log4c.h"
#include "JNICallbakcHelper.h"
#include "BKPlayer.h"
#include <android/native_window_jni.h> // ANativeWindow 用来渲染画面的 == Surface对象

extern "C" {
#include <libavutil/avutil.h>
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_wulala_bkplayerer_MainActivity_stringFromJNI(JNIEnv *env, jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

JavaVM *vm = nullptr;
ANativeWindow *window = nullptr;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // 静态初始化 锁

jint JNI_OnLoad(JavaVM *vm, void *args) {
    ::vm = vm;
    return JNI_VERSION_1_6;
}

// 函数指针的实现 实现渲染画面
void renderFrame(uint8_t *src_data, int width, int height, int src_linesize) {
    pthread_mutex_lock(&mutex);
    if (!window) {
        pthread_mutex_unlock(&mutex); // 出现了问题后，必须考虑到，释放锁，怕出现死锁问题
    }

    // 设置窗口的大小，各个属性
    ANativeWindow_setBuffersGeometry(window, width, height, WINDOW_FORMAT_RGBA_8888);

    // 他自己有个缓冲区 buffer
    ANativeWindow_Buffer window_buffer;

    // 如果我在渲染的时候，是被锁住的，那我就无法渲染，我需要释放 ，防止出现死锁
    if (ANativeWindow_lock(window, &window_buffer, 0)) {
        ANativeWindow_release(window);
        window = 0;

        pthread_mutex_unlock(&mutex); // 解锁，怕出现死锁
        return;
    }

    // 填充[window_buffer]  画面就出来了  ==== 【目标 window_buffer】
    uint8_t *dst_data = static_cast<uint8_t *>(window_buffer.bits);
    int dst_linesize = window_buffer.stride * 4;

    for (int i = 0; i < window_buffer.height; ++i) { // 图：一行一行显示 [高度不用管，用循环了，遍历高度]

        // 通用的
        memcpy(dst_data + i * dst_linesize, src_data + i * src_linesize, dst_linesize); // OK的

        // LOGD("dst_linesize:%d  src_linesize:%d\n", dst_linesize, src_linesize)
    }

    // 数据刷新
    ANativeWindow_unlockAndPost(window); // 解锁后 并且刷新 window_buffer的数据显示画面

    pthread_mutex_unlock(&mutex);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_wulala_bkplayer_BKPlayer_prepareNative(JNIEnv *env, jobject job, jstring data_source) {
    const char *data_source_ = env->GetStringUTFChars(data_source, nullptr);
    auto *helper = new JNICallbakcHelper(vm, env, job); // C++子线程回调 ， C++主线程回调
    auto *player = new BKPlayer(data_source_, helper); // 有意为之的，开辟堆空间，不能释放
    player->setRenderCallback(renderFrame);
    player->prepare();
    env->ReleaseStringUTFChars(data_source, data_source_);
    return reinterpret_cast<jlong>(player);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_wulala_bkplayer_BKPlayer_startNative(JNIEnv *env, jobject thiz, jlong nativeObj) {
    auto *player = reinterpret_cast<BKPlayer *>(nativeObj);
    if (player) {
        player->start();
    }else{
        LOGE("player is null");
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_wulala_bkplayer_BKPlayer_stopNative(JNIEnv *env, jobject thiz, jlong nativeObj) {
    auto *player = reinterpret_cast<BKPlayer *>(nativeObj);
    if (player) {
        player->stop();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_wulala_bkplayer_BKPlayer_releaseNative(JNIEnv *env, jobject thiz, jlong nativeObj) {
    auto *player = reinterpret_cast<BKPlayer *>(nativeObj);

    pthread_mutex_lock(&mutex);

    // 先释放之前的显示窗口
    if (window) {
        ANativeWindow_release(window);
        window = nullptr;
    }

    pthread_mutex_unlock(&mutex);

    // 释放工作
    DELETE(player); // 在堆区开辟的 DerryPlayer.cpp 对象，已经被释放了哦
    DELETE(vm);
    DELETE(window);
}


// 实例化出 window
extern "C"
JNIEXPORT void JNICALL
Java_com_wulala_bkplayer_BKPlayer_setSurfaceNative(JNIEnv *env, jobject thiz, jobject surface, jlong nativeObj) {
    auto *player = reinterpret_cast<BKPlayer *>(nativeObj);

    pthread_mutex_lock(&mutex);

    // 先释放之前的显示窗口
    if (window) {
        ANativeWindow_release(window);
        window = nullptr;
    }

    // 创建新的窗口用于视频显示
    window = ANativeWindow_fromSurface(env, surface);

    pthread_mutex_unlock(&mutex);
}

// TODO 第七节课增加 获取总时长
extern "C"
JNIEXPORT jint JNICALL
Java_com_wulala_bkplayer_BKPlayer_getDurationNative(JNIEnv *env, jobject thiz, jlong nativeObj) {
    auto *player = reinterpret_cast<BKPlayer *>(nativeObj);
    if (player) {
        return player->getDuration();
    }
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_wulala_bkplayer_BKPlayer_seekNative(JNIEnv *env, jobject thiz, jint play_value, jlong nativeObj) {
    auto *player = reinterpret_cast<BKPlayer *>(nativeObj);
    if (player) {
        player->seek(play_value);
    }
}
