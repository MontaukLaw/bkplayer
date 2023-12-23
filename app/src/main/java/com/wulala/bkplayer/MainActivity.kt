package com.wulala.bkplayer

import android.Manifest
import android.annotation.SuppressLint
import android.content.pm.PackageManager
import android.graphics.Color
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.os.Environment
import android.view.SurfaceView
import android.view.View
import android.widget.Button
import android.widget.SeekBar
import android.widget.TextView
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.wulala.bkplayer.databinding.ActivityMainBinding
import java.io.File

class MainActivity : AppCompatActivity(), SeekBar.OnSeekBarChangeListener {

    private lateinit var binding: ActivityMainBinding

    private var player: BKPlayer? = null
    private var tvState: TextView? = null
    private var surfaceView: SurfaceView? = null

    private var seekBar: SeekBar? = null // 拖动条
    private var tvTime: TextView? = null // 显示播放时间
    private var isTouch = false // 用户是否拖拽了 拖动条，（默认是没有拖动false）
    private var duration = 0 // 获取native层的总时长


    private var startBtn: Button? = null
    private var stopBtn: Button? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        tvState = binding.tvState
        surfaceView = binding.surfaceView

        tvTime = binding.tvTime
        seekBar = binding.seekBar
        startBtn = binding.btnStart
        stopBtn = binding.btnStop

        seekBar?.setOnSeekBarChangeListener(this);
        player = BKPlayer()
        lifecycle.addObserver(player!!) // MainActivity被观察者 与 BKPlayer观察者 建立绑定关系
        player?.setSurfaceView(surfaceView!!)
        player?.setDataSource(
            File(
                Environment.getExternalStorageDirectory(), "demo.mp4"
            ).absolutePath
        )

        player!!.setOnErrorListener(object : BKPlayer.OnErrorListener {

            @SuppressLint("SetTextI18n")
            override fun onError(errorCode: String?) {
                runOnUiThread { // Toast.makeText(MainActivity.this, "出错了，错误详情是:" + errorInfo, Toast.LENGTH_SHORT).show();
                    tvState?.setTextColor(Color.RED) // 红色
                    tvState?.text = errorCode
                }
            }
        })

        startBtn?.setOnClickListener {
            player?.start()
        }

        stopBtn?.setOnClickListener {
            player?.stop()
        }

        player!!.setOnPreparedListener(object : BKPlayer.OnPreparedListener {

            @SuppressLint("SetTextI18n")
            override fun onPrepared() {

                // 得到视频总时长： 直播：duration=0，  非直播-视频：duration=有值的
                duration = player!!.duration

                runOnUiThread {
                    if (duration != 0) {

                        // duration == 119 转换成  01:59
                        // 非直播-视频
                        // tv_time.setText("00:00/" + "01:59");
                        tvTime!!.text = "00:00/" + getMinutes(duration) + ":" + getSeconds(duration)
                        tvTime?.visibility = View.VISIBLE // 显示
                        seekBar!!.visibility = View.VISIBLE // 显示
                    }

                    // Toast.makeText(MainActivity.this, "准备成功，即将开始播放", Toast.LENGTH_SHORT).show();
                    tvState?.setTextColor(Color.GREEN) // 绿色
                    tvState!!.text = "恭喜init初始化成功"
                }

                player!!.start() // 调用 C++ 开始播放
            }
        })

        player!!.setOnOnProgressListener(object : BKPlayer.OnProgressListener {

            @SuppressLint("SetTextI18n")
            override fun onProgress(progress: Int) {

                // 【如果是人为拖动的，不能干预我们计算】 否则会混乱
                if (!isTouch) {

                    // C++层是异步线程调用上来的，小心，UI
                    runOnUiThread {
                        if (duration != 0) {
                            // progress:C++层 ffmpeg获取的当前播放【时间（单位是秒 80秒都有，肯定不符合界面的显示） -> 1分20秒】
                            tvTime!!.text = (getMinutes(progress) + ":" + getSeconds(progress) + "/" + getMinutes(duration) + ":" + getSeconds(duration))
                            // progress == C++层的 音频时间搓  ----> seekBar的百分比
                            // seekBar.setProgress(progress * 100 / duration 以秒计算seekBar相对总时长的百分比);
                            seekBar!!.progress = progress * 100 / duration
                        }
                    }
                }
            }
        })

        // 动态 6.0及以上的 申请权限
        checkPermission()
    }

    private fun getMinutes(duration: Int): String { // 给我一个duration，转换成xxx分钟
        val minutes = duration / 60
        return if (minutes <= 9) {
            "0$minutes"
        } else "" + minutes
    }

    // 119 ---> 60 59
    private fun getSeconds(duration: Int): String { // 给我一个duration，转换成xxx秒
        val seconds = duration % 60
        return if (seconds <= 9) {
            "0$seconds"
        } else "" + seconds
    }

    @SuppressLint("SetTextI18n")
    override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
        if (fromUser) {
            // progress 是进度条的进度 （0 - 100） ------>   秒 分 的效果
            tvTime!!.text = (getMinutes(progress * duration / 100)
                    + ":" +
                    getSeconds(progress * duration / 100) + "/" +
                    getMinutes(duration) + ":" + getSeconds(duration))
        }
    }

    // 手按下去，回调此函数
    override fun onStartTrackingTouch(seekBar: SeekBar?) {
        isTouch = true
    }

    // 手松开（SeekBar当前值 ---> C++层），回调此函数
    override fun onStopTrackingTouch(seekBar: SeekBar) {
        isTouch = false
        val seekBarProgress = seekBar.progress // 获取当前seekbar当前进度

        // SeekBar1~100  -- 转换 -->  C++播放的时间（61.546565）
        val playProgress = seekBarProgress * duration / 100
        player!!.seek(playProgress)
    }

    private var permissions = arrayOf<String>(Manifest.permission.WRITE_EXTERNAL_STORAGE) // 如果要申请多个动态权限，此处可以写多个

    var mPermissionList: MutableList<String> = ArrayList()

    private val PERMISSION_REQUEST = 1

    // 检查权限
    private fun checkPermission() {
        mPermissionList.clear()

        // 判断哪些权限未授予
        for (permission in permissions) {
            if (ContextCompat.checkSelfPermission(this, permission) != PackageManager.PERMISSION_GRANTED) {
                mPermissionList.add(permission)
            }
        }

        // 判断是否为空
        if (mPermissionList.isEmpty()) { // 未授予的权限为空，表示都授予了
        } else {
            //请求权限方法
            val permissions = mPermissionList.toTypedArray() //将List转为数组
            ActivityCompat.requestPermissions(this, permissions, PERMISSION_REQUEST)
        }
    }

    /**
     * 响应授权
     * 这里不管用户是否拒绝，都进入首页，不再重复申请权限
     */
    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String?>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        when (requestCode) {
            PERMISSION_REQUEST -> {}
            else -> super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        }
    }

}