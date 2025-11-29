package net.sorayuki.featuretest

import android.os.Bundle
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import android.view.View
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
                val devName = cl.QueryString(cl.self, "device_name")
                val platName = cl.QueryString(cl.self, "platform_name")
                fgHandler.post {
                    binding.clDeviceName.text = devName
                    binding.clPlatformName.text = platName
                    binding.mainOpLayout.visibility = View.VISIBLE
                }
            }
        }


        val copyTest = fun(useKernel: Boolean, it: View) {
            it.isEnabled = false
            bgHandler.post {
                val speed = cl.TestCopy(cl.self, useKernel)
                fgHandler.post {
                    binding.testD2DResult.text = "%.2f MB/s".format(speed)
                    it.isEnabled = true
                }
            }
        }

        binding.testAPICopy.setOnClickListener { copyTest(false, it) }
        binding.testKernelCopy.setOnClickListener { copyTest(true, it) }
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
    external fun TestCopy(self: Long, useKernel: Boolean): Float
}