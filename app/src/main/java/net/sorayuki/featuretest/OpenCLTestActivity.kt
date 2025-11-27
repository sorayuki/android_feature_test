package net.sorayuki.featuretest

import android.os.Bundle
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import net.sorayuki.featuretest.databinding.ActivityOpenCltestBinding
import java.io.Closeable

class OpenCLTestActivity : AppCompatActivity() {
    private lateinit var binding: ActivityOpenCltestBinding
    private lateinit var fgHandler: Handler
    private lateinit var bgThread: HandlerThread
    private lateinit var bgHandler: Handler

    lateinit var cl: OpenCLTest

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        fgHandler = Handler(Looper.myLooper()!!)
        bgThread = HandlerThread("worker")
        bgThread.start()
        bgHandler = Handler(bgThread.looper)

        enableEdgeToEdge()
        binding = ActivityOpenCltestBinding.inflate(layoutInflater)
        setContentView(binding.root)
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        bgHandler.post {
            cl = OpenCLTest()
            if (cl.Init(cl.self)) {
                cl.QueryString(cl.self, "device_name")?.let {
                    fgHandler.post {
                        binding.clDeviceName.text = it
                    }
                }
                cl.QueryString(cl.self, "platform_name")?.let {
                    fgHandler.post {
                        binding.clPlatformName.text = it
                    }
                }
            }
        }

        binding.testD2DBtn.setOnClickListener {
            binding.testD2DBtn.isEnabled = false
            bgHandler.post {
                val costMs = cl.TestHostToDeviceTransfer(cl.self, 20)
                val speed = 400.0f / (costMs / 1000.0f)
                fgHandler.post {
                    binding.testD2DResult.text = "%.2f MB/s".format(speed)
                    binding.testD2DBtn.isEnabled = true
                }
            }
        }
    }
}

class OpenCLTest: Closeable {
    init {
        System.loadLibrary("featuretest")
    }

    var self: Long = 0

    constructor() {
        self = Create()
    }

    protected fun finalize() {
        close()
    }

    override fun close() {
        if (self != 0L) {
            Delete(self)
            self = 0L
        }
    }

    external fun Create(): Long
    external fun Delete(self: Long)
    external fun Init(self: Long): Boolean
    // 1 = device, 2 = platform
    external fun QueryString(self: Long, key: String): String
    external fun TestHostToDeviceTransfer(self: Long, times_400MB: Int): Long
}