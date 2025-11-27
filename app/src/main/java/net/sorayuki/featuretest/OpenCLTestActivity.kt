package net.sorayuki.featuretest

import android.os.Bundle
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import net.sorayuki.featuretest.databinding.ActivityOpenCltestBinding
import java.io.Closeable

class OpenCLTestActivity : AppCompatActivity() {
    private lateinit var binding: ActivityOpenCltestBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        binding = ActivityOpenCltestBinding.inflate(layoutInflater)
        setContentView(binding.root)
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        if (cl.nativeInit(cl.self)) {
            binding.clDeviceName.text = cl.queryString(cl.self, "device_name")
            binding.clPlatformName.text = cl.queryString(cl.self, "platform_name")
        }
    }

    val cl = OpenCLTest()
}

class OpenCLTest: Closeable {
    init {
        System.loadLibrary("featuretest")
    }

    var self: Long = 0

    constructor() {
        self = nativeCreate()
    }

    protected fun finalize() {
        close()
    }

    override fun close() {
        if (self != 0L) {
            nativeDelete(self)
            self = 0L
        }
    }

    external fun nativeCreate(): Long
    external fun nativeDelete(self: Long)
    external fun nativeInit(self: Long): Boolean
    // 1 = device, 2 = platform
    external fun queryString(self: Long, key: String): String
}