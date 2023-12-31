#include "include/BKPlayer.h"

BKPlayer::BKPlayer(const char *data_source, JNICallbakcHelper *helper) {
    // this->data_source = data_source;
    // 如果被释放，会造成悬空指针

    // 深拷贝
    // this->data_source = new char[strlen(data_source)];
    // Java: demo.mp4
    // C层：demo.mp4\0  C层会自动 + \0,  strlen不计算\0的长度，所以我们需要手动加 \0

    this->data_source = new char[strlen(data_source) + 1];
    strcpy(this->data_source, data_source); // 把源 Copy给成员

    this->helper = helper;

    // pthread_mutex_init(&seek_mutex, nullptr);
}

// 这个object
BKPlayer::~BKPlayer() {

    if (data_source) {
        delete data_source;
        data_source = nullptr;
    }

    if (helper) {
        delete helper;
        helper = nullptr;
    }

    // pthread_mutex_destroy(&seek_mutex);
}

// void* (*__start_routine)(void*)  子线程了
void *task_prepare(void *args) { // 此函数和DerryPlayer这个对象没有关系，你没法拿DerryPlayer的私有成员

    // avformat_open_input(0, this->data_source)

    auto *player = static_cast<BKPlayer *>(args);
    player->prepare_();
    return nullptr; // 必须返回，坑，错误很难找
}

void BKPlayer::prepare_() { // 属于 子线程了 并且 拥有  DerryPlayer的实例 的 this

    // 为什么FFmpeg源码，大量使用上下文Context？
    // 答：因为FFmpeg源码是纯C的，他不像C++、Java ， 上下文的出现是为了贯彻环境，就相当于Java的this能够操作成员

    formatContext = avformat_alloc_context();

    // 字典（键值对）
    AVDictionary *dictionary = nullptr;
    //设置超时（5秒）
    av_dict_set(&dictionary, "timeout", "5000000", 0); // 单位微妙

    /**
     * 1，AVFormatContext *
     * 2，路径 url:文件路径或直播地址
     * 3，AVInputFormat *fmt  Mac、Windows 摄像头、麦克风， 我们目前安卓用不到
     * 4，各种设置：例如：Http 连接超时， 打开rtmp的超时  AVDictionary **options
     */
    int r = avformat_open_input(&formatContext, data_source, nullptr, &dictionary);
    // 释放字典
    av_dict_free(&dictionary);
    if (r) {
        // 把错误信息反馈给Java，回调给Java  Toast【打开媒体格式失败，请检查代码】
        if (helper) {
            helper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_OPEN_URL, av_err2str(r));

            // char * errorInfo = av_err2str(r); // 根据你的返回值 得到错误详情
        }
        avformat_close_input(&formatContext);
        return;
    }

    // 你在 xxx.mp4 能够拿到
    // 你在 xxx.flv 拿不到，是因为封装格式的原因
    // formatContext->duration;

    r = avformat_find_stream_info(formatContext, nullptr);
    if (r < 0) {
        if (helper) {
            helper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_FIND_STREAMS, av_err2str(r));
        }
        avformat_close_input(&formatContext);
        return;
    }

    // 你在 xxx.mp4 能够拿到
    // 你在 xxx.flv 都能拿到
    // avformat_find_stream_info FFmpeg内部源码已经做（流探索）了，所以可以拿到 总时长
    // this->duration = formatContext->duration / AV_TIME_BASE; // FFmpeg的单位 基本上都是  有理数(时间基)，所以你需要这样转

    AVCodecContext *codecContext = nullptr;

    for (int stream_index = 0; stream_index < formatContext->nb_streams; ++stream_index) {
        AVStream *stream = formatContext->streams[stream_index];

        AVCodecParameters *parameters = stream->codecpar;

        AVCodec *codec = const_cast<AVCodec *>(avcodec_find_decoder(parameters->codec_id));
        if (!codec) {
            if (helper) {
                helper->onError(THREAD_CHILD, FFMPEG_FIND_DECODER_FAIL, av_err2str(r));
            }
            avformat_close_input(&formatContext);
        }

        codecContext = avcodec_alloc_context3(codec);
        if (!codecContext) {
            if (helper) {
                helper->onError(THREAD_CHILD, FFMPEG_ALLOC_CODEC_CONTEXT_FAIL, av_err2str(r));
            }

            avcodec_free_context(&codecContext); // 释放此上下文 avcodec 他会考虑到，你不用管*codec
            avformat_close_input(&formatContext);

            return;
        }

        r = avcodec_parameters_to_context(codecContext, parameters);
        if (r < 0) {
            if (helper) {
                helper->onError(THREAD_CHILD, FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL, av_err2str(r));
            }
            avcodec_free_context(&codecContext); // 释放此上下文 avcodec 他会考虑到，你不用管*codec
            avformat_close_input(&formatContext);
            return;
        }

        r = avcodec_open2(codecContext, codec, nullptr);
        if (r) { // 非0就是true
            if (helper) {
                helper->onError(THREAD_CHILD, FFMPEG_OPEN_DECODER_FAIL, av_err2str(r));
            }
            avcodec_free_context(&codecContext); // 释放此上下文 avcodec 他会考虑到，你不用管*codec
            avformat_close_input(&formatContext);
            return;
        }

        AVRational time_base = stream->time_base;

        if (parameters->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO) {

            // 虽然是视频类型，但是只有一帧封面
            if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                continue;
            }

            AVRational fps_rational = stream->avg_frame_rate;
            int fps = av_q2d(fps_rational);

            // 是视频
            video_channel = new VideoChannel(stream_index, codecContext, time_base, fps);
            video_channel->setRenderCallback(renderCallback);
            video_channel->setJNICallbakcHelper(helper);
        }
    } // for end

    if (!video_channel) {
        if (helper) {
            helper->onError(THREAD_CHILD, FFMPEG_NOMEDIA, av_err2str(r));
        }
        if (codecContext) {
            avcodec_free_context(&codecContext); // 释放此上下文 avcodec 他会考虑到，你不用管*codec
        }
        avformat_close_input(&formatContext);
        return;
    }
    if (helper) { // 只要用户关闭了，就不准你回调给Java成 start播放
        helper->onPrepared(THREAD_CHILD);
    }
}

void BKPlayer::prepare() {
    // 问题：当前的prepare函数，子线程，还是，主线程？
    // 此函数是被MainActivity的onResume调用下来的（安卓的主线程）

    // 解封装 FFMpeg来解析   data_source 可以直接解析吗？
    // 答：data_source == 文件IO流  ，直播网络rtmp，  所以按道理来说，会耗时，所以必须使用子线程

    // 创建子线程 pthread
    pthread_create(&pid_prepare, nullptr, task_prepare, this); // this == DerryPlayer的实例
}

void *task_start(void *args) {
    auto *player = static_cast<BKPlayer *>(args);
    if (player) {
        player->start_();
    } else {
        LOGE("task_start player is null");
    }
    return nullptr; // 必须返回，坑，错误很难找
}

// 把 视频 音频 的 压缩包（AVPacket *） 循环获取出来 加入到队列里面去
void BKPlayer::start_() { // 子线程
    // 一股脑 把 AVPacket * 丢到 队列里面去  不区分 音频 视频
    LOGD("start_");
    while (isPlaying) {
        // 解决方案：视频 我不丢弃数据，等待队列中数据 被消费 内存泄漏点1.1
        if (video_channel && video_channel->packets.size() > 10) {
            av_usleep(10 * 1000); // 单位：microseconds 微妙 10毫秒
            LOGD("video_channel->packets.size() > 10");
            continue;
        }

        // LOGD("av_read_frame");

        // AVPacket 可能是音频 也可能是视频（压缩包）
        AVPacket *packet = av_packet_alloc();

        int ret = av_read_frame(formatContext, packet);
        if (!ret) { // ret == 0
            // AudioChannel 队列   packages队列（AVPacket * 压缩）    frames队列 （AVFrame * 原始）
            // VideoChannel 队列   packages队列（AVPacket * 压缩）    frames队列 （AVFrame * 原始）
            // 把我们的 AVPacket* 加入队列， 音频 和 视频
            /*AudioChannel.insert(packet);
            VideioChannel.insert(packet);*/

            if (video_channel && video_channel->stream_index == packet->stream_index) {
                // 代表是视频
                // LOGD("Insert video packet to queue");
                video_channel->packets.insertToQueue(packet);
            }
        } else if (ret == AVERROR_EOF) { //   end of file == 读到文件末尾了 == AVERROR_EOF
            if (video_channel->packets.empty()) {
                LOGD("Packet is empty");
                break; // 队列的数据被音频 视频 全部播放完毕了，我再退出
            }
        } else {
            LOGD("av_read_frame error");
            break; // av_read_frame(formatContext,  packet); 出现了错误，结束当前循环
        }
    }
    // end while
    isPlaying = false;
    // 事实上实在这里stop video的playing
    video_channel->stop();
}

void BKPlayer::start() {
    LOGD("start");
    isPlaying = 1;

    // 视频：1.解码    2.播放
    // 1.把队列里面的压缩包(AVPacket *)取出来，然后解码成（AVFrame * ）原始包 ----> 保存队列
    // 2.把队列里面的原始包(AVFrame *)取出来， 视频播放
    if (video_channel) {
        video_channel->start(); // 视频的播放
    } else {
        LOGE("video_channel is null");
    }
    LOGD("start task start thread");
    // 把 音频 视频 压缩包  加入队列里面去
    // 创建子线程 pthread
    pthread_create(&pid_start, nullptr, task_start, this); // this == DerryPlayer的实例
}

void BKPlayer::setRenderCallback(RenderCallback renderCallback_) {
    this->renderCallback = renderCallback_;
}


#if 0

int BKPlayer::getDuration() {
    return duration; // 在调用此函数之前，必须给此duration变量赋值
}

void BKPlayer::seek(int progress) {

    // 健壮性判断
    if (progress < 0 || progress > duration) {
        return;
    }
    if (!video_channel) {
        return;
    }
    if (!formatContext) {
        return;
    }

    // formatContext 多线程， av_seek_frame内部会对我们的 formatContext上下文的成员做处理，安全的问题
    // 互斥锁 保证多线程情况下安全
    pthread_mutex_lock(&seek_mutex);

    // FFmpeg 大部分单位 == 时间基AV_TIME_BASE
    /**
     * 1.formatContext 安全问题
     * 2.-1 代表默认情况，FFmpeg自动选择 音频 还是 视频 做 seek，  模糊：0视频  1音频
     * 3. AVSEEK_FLAG_ANY（老实） 直接精准到 拖动的位置，问题：如果不是关键帧，B帧 可能会造成 花屏情况
     *    AVSEEK_FLAG_BACKWARD（则优  8的位置 B帧 ， 找附件的关键帧 6，如果找不到他也会花屏）
     *    AVSEEK_FLAG_FRAME 找关键帧（非常不准确，可能会跳的太多），一般不会直接用，但是会配合用
     */
    int r = av_seek_frame(formatContext, -1, progress * AV_TIME_BASE, AVSEEK_FLAG_FRAME);
    if (r < 0) {
        return;
    }

    // 有一点点冲突，后面再看 （则优  | 配合找关键帧）
    // av_seek_frame(formatContext, -1, progress * AV_TIME_BASE, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);

    // 音视频正在播放，用户去 seek，我是不是应该停掉播放的数据  音频1frames 1packets，  视频1frames 1packets 队列

    if (video_channel) {
        video_channel->packets.setWork(0);  // 队列不工作
        video_channel->frames.setWork(0);  // 队列不工作
        video_channel->packets.clear();
        video_channel->frames.clear();
        video_channel->packets.setWork(1); // 队列继续工作
        video_channel->frames.setWork(1);  // 队列继续工作
    }

    // pthread_mutex_unlock(&seek_mutex);

}
#endif

void *task_stop(void *args) {
    auto *player = static_cast<BKPlayer *>(args);
    player->stop_(player);
    return nullptr;
}

void BKPlayer::stop_(BKPlayer *derryPlayer) {

    this->isPlaying = false;

    pthread_join(pid_prepare, nullptr);
    pthread_join(pid_start, nullptr);

    // pid_prepare pid_start 就全部停止下来了  稳稳的停下来
    if (formatContext) {
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        formatContext = nullptr;
    }
    DELETE(video_channel);
    DELETE(derryPlayer);
}

void BKPlayer::stop() {

    // 只要用户关闭了，就不准你回调给Java成 start播放
    DELETE(helper)
    // helper = nullptr;
    if (video_channel) {
        video_channel->jniCallbakcHelper = nullptr;
    }

    // 如果是直接释放 我们的 prepare_ start_ 线程，不能暴力释放 ，否则会有bug

    // 让他 稳稳的停下来

    // 我们要等这两个线程 稳稳的停下来后，我再释放DerryPlayer的所以工作
    // 由于我们要等 所以会ANR异常

    // 所以我们我们在开启一个 stop_线程 来等你 稳稳的停下来
    // 创建子线程
    pthread_create(&pid_stop, nullptr, task_stop, this);

}







