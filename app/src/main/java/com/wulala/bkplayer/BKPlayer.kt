package com.wulala.bkplayer

import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent

class BKPlayer : SurfaceHolder.Callback, LifecycleObserver {
    val TAG = "BK Player"
    companion object {
        init {
            System.loadLibrary("bkplayer")
        }
    }

    private var onPreparedListener: OnPreparedListener? = null // C++层准备情况的接口
    private var nativeObj: Long? = null // 保存DerryPlayer.cpp对象的地址
    private var onErrorListener: OnErrorListener? = null
    private var surfaceHolder: SurfaceHolder? = null

    // 媒体源（文件路径， 直播地址rtmp）
    private var dataSource: String? = null
    fun setDataSource(dataSource: String?) {
        this.dataSource = dataSource
    }


    /**
     * 播放前的 准备工作 // ActivityThread.java Handler
     */
    @OnLifecycleEvent(Lifecycle.Event.ON_RESUME)
    fun prepare() { // 我们的准备工作：触发
        Log.d(TAG, "prepare: ")
        nativeObj = prepareNative(dataSource!!)
    }

    /**
     * 开始播放，需要准备成功后 由MainActivity哪里调用
     */
    fun start() {
        startNative(nativeObj!!)
    }

    /**
     * 停止播放
     */
    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun stop() {
        stopNative(nativeObj!!)
    }

    /**
     * 释放资源
     */
    @OnLifecycleEvent(Lifecycle.Event.ON_DESTROY)
    fun release() {
        releaseNative(nativeObj!!)
    }

    // 写一个函数，给C++调用
    /**
     * 给jni反射调用的  准备成功
     */
    fun onPrepared() {
        if (onPreparedListener != null) {
            onPreparedListener!!.onPrepared()
        }
    }

    /**
     * 获取总时长
     * @return
     */
    val duration: Int get() = getDurationNative(nativeObj!!)

    /**
     * MainActivity 调用 seek 拖动 ，拖动条控制C++代码
     */
    fun seek(playProgress: Int) {
        seekNative(playProgress, nativeObj!!)
    }

    // 更多更多的 方法，需要给  C++ 调用，所以  有可能是   C++子线程调用   C++主线程调用

    /**
     * 准备OK的监听接口
     */
    interface OnPreparedListener {
        fun onPrepared()
    }

    /**
     * 设置准备OK的监听接口
     */
    fun setOnPreparedListener(onPreparedListener: OnPreparedListener?) {
        this.onPreparedListener = onPreparedListener
    }

    /**
     * 给jni反射调用的 准备错误了
     */
    fun onError(errorCode: Int, ffmpegError: String) {
        val title = "\nFFmpeg给出的错误如下:\n"
        if (null != onErrorListener) {
            var msg: String? = null
            when (errorCode) {
                FFMPEG.FFMPEG_CAN_NOT_OPEN_URL -> msg = "打不开视频$title$ffmpegError"
                FFMPEG.FFMPEG_CAN_NOT_FIND_STREAMS -> msg = "找不到流媒体$title$ffmpegError"
                FFMPEG.FFMPEG_FIND_DECODER_FAIL -> msg = "找不到解码器$title$ffmpegError"
                FFMPEG.FFMPEG_ALLOC_CODEC_CONTEXT_FAIL -> msg = "无法根据解码器创建上下文$title$ffmpegError"
                FFMPEG.FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL -> msg = "根据流信息 配置上下文参数失败$title$ffmpegError"
                FFMPEG.FFMPEG_OPEN_DECODER_FAIL -> msg = "打开解码器失败$title$ffmpegError"
                FFMPEG.FFMPEG_NOMEDIA -> msg = "没有音视频$title$ffmpegError"
            }
            onErrorListener!!.onError(msg)
        }
    }

    /**
     * 准备失败错误异常的接口监听
     */
    interface OnErrorListener {
        fun onError(errorCode: String?)
    }

    /**
     * 设置准备失败错误异常的接口监听
     */
    fun setOnErrorListener(onErrorListener: OnErrorListener?) {
        this.onErrorListener = onErrorListener
    }

    /**
     * set SurfaceView
     * @param surfaceView
     */
    fun setSurfaceView(surfaceView: SurfaceView) {
        if (surfaceHolder != null) {
            surfaceHolder?.removeCallback(this) // 清除上一次的
        }
        surfaceHolder = surfaceView.holder
        surfaceHolder?.addCallback(this) // 监听
    }

    override fun surfaceCreated(surfaceHolder: SurfaceHolder) {
        // setSurfaceNative
    }

    // 界面发生了改变
    override fun surfaceChanged(surfaceHolder: SurfaceHolder, format: Int, width: Int, height: Int) {
        setSurfaceNative(surfaceHolder.surface, nativeObj!!)
    }

    override fun surfaceDestroyed(surfaceHolder: SurfaceHolder) {}

    /**
     * 给jni反射调用的  准备成功
     */
    fun onProgress(progress: Int) {
        if (onProgressListener != null) {
            onProgressListener!!.onProgress(progress)
        }
    }

    private var onProgressListener: OnProgressListener? = null

    /**
     * 准备播放时进度的监听接口
     */
    interface OnProgressListener {
        fun onProgress(progress: Int)
    }

    /**
     * 设置准备播放时进度的监听接口
     */
    fun setOnOnProgressListener(onProgressListener: OnProgressListener?) {
        this.onProgressListener = onProgressListener
    }

    private external fun prepareNative(dataSource: String): Long
    private external fun startNative(nativeObj: Long)
    private external fun stopNative(nativeObj: Long)
    private external fun releaseNative(nativeObj: Long)
    private external fun setSurfaceNative(surface: Surface, nativeObj: Long)
    private external fun getDurationNative(nativeObj: Long): Int
    private external fun seekNative(playValue: Int, nativeObj: Long)

}