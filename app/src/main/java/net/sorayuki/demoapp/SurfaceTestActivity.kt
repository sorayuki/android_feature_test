package net.sorayuki.demoapp

import android.graphics.ColorSpace
import android.opengl.*
import android.os.Bundle
import android.os.Handler
import android.os.HandlerThread
import android.renderscript.ScriptGroup
import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import net.sorayuki.demoapp.databinding.ActivitySurfacetestBinding

class SurfaceTestActivity : AppCompatActivity() {
    
    lateinit var bgThread: HandlerThread
    lateinit var bgHandler: Handler

    var frame: YUVFrame? = null

    lateinit var surface: Surface

    lateinit var binding: ActivitySurfacetestBinding

    val EGL_GL_COLORSPACE_DISPLAY_P3_EXT = 0x3363
    val EGL_GL_COLORSPACE_SCRGB_LINEAR_EXT = 0x3350
    val EGL_GL_COLORSPACE_BT2020_PQ_EXT = 0x3340
    val EGL_GL_COLORSPACE_BT2020_HLG_EXT = 0x3540

    val EGL_COLOR_COMPONENT_TYPE_EXT = 0x3339
    val EGL_COLOR_COMPONENT_TYPE_FIXED_EXT = 0x333A
    val EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT = 0x333B

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        binding = ActivitySurfacetestBinding.inflate(layoutInflater)
        setContentView(binding.root)
        ViewCompat.setOnApplyWindowInsetsListener(binding.main) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        binding.surfaceView.holder.addCallback(object: SurfaceHolder.Callback {
            override fun surfaceChanged(
                holder: SurfaceHolder,
                format: Int,
                width: Int,
                height: Int
            ) {
                surface = holder.surface
            }
            override fun surfaceCreated(holder: SurfaceHolder) {
                surface = holder.surface
            }
            override fun surfaceDestroyed(holder: SurfaceHolder) {
            }
        })

        bgThread = HandlerThread("worker")
        bgThread.start()
        bgHandler = Handler(bgThread.looper)
        
        bgHandler.post {
            val bitstream = resources.openRawResource(R.raw.aqua).readBytes()
            frame = HEVCDecoder.decode(bitstream)
        }

        binding.btnSRGB.setOnClickListener {
            bgHandler.post {
                draw(8, EGL15.EGL_GL_COLORSPACE_SRGB, 0.0f, 0.0f, 0.0f)
                draw(8, EGL15.EGL_GL_COLORSPACE_SRGB, 1.0f, 0.0f, 0.0f)
            }
        }
        binding.btnP3.setOnClickListener {
            bgHandler.post {
                draw(8, EGL15.EGL_GL_COLORSPACE_SRGB, 0.0f, 0.0f, 0.0f)
                draw(8, EGL_GL_COLORSPACE_DISPLAY_P3_EXT, 1.0f, 0.0f, 0.0f)
            }
        }
        binding.btnScRGB.setOnClickListener {
            bgHandler.post {
                draw(8, EGL15.EGL_GL_COLORSPACE_SRGB, 0.0f, 0.0f, 0.0f)
                draw(16, EGL_GL_COLORSPACE_SCRGB_LINEAR_EXT, 1.0f, 0.0f, 0.0f)
            }
        }
        binding.btnHLG.setOnClickListener {
            bgHandler.post {
                draw(8, EGL15.EGL_GL_COLORSPACE_SRGB, 0.0f, 0.0f, 0.0f)
                draw(10, EGL_GL_COLORSPACE_BT2020_HLG_EXT, 1.0f, 0.0f, 0.0f)
            }
        }
        binding.btnPQ.setOnClickListener {
            bgHandler.post {
                draw(8, EGL15.EGL_GL_COLORSPACE_SRGB, 0.0f, 0.0f, 0.0f)
                draw(10, EGL_GL_COLORSPACE_BT2020_PQ_EXT, 1.0f, 0.0f, 0.0f)
            }
        }
    }

    var eglDisplay: EGLDisplay = EGL14.EGL_NO_DISPLAY
    var eglSurface: EGLSurface = EGL14.EGL_NO_SURFACE
    var eglContext: EGLContext = EGL14.EGL_NO_CONTEXT

    fun draw(bits: Int, colorspace: Int, red: Float, green: Float, blue: Float) {
        if (eglDisplay == EGL14.EGL_NO_DISPLAY) {
            eglDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY)
            val eglVer = IntArray(2)
            if (!EGL14.eglInitialize(eglDisplay, eglVer, 0, eglVer, 1))
                throw RuntimeException("Fail to init egl environment")
        }

        if (eglSurface != EGL14.EGL_NO_SURFACE) {
            EGL14.eglDestroySurface(eglDisplay, eglSurface)
            eglSurface = EGL14.EGL_NO_SURFACE
        }
        if (eglContext != EGL14.EGL_NO_CONTEXT) {
            EGL14.eglDestroyContext(eglDisplay, eglContext)
            eglContext = EGL14.EGL_NO_CONTEXT
        }

        val configAttr = intArrayOf(
            EGL14.EGL_SURFACE_TYPE, EGL14.EGL_WINDOW_BIT,
            EGL14.EGL_RENDERABLE_TYPE, EGL15.EGL_OPENGL_ES3_BIT,
            EGL14.EGL_COLOR_BUFFER_TYPE, EGL14.EGL_RGB_BUFFER,
            EGL14.EGL_RED_SIZE, bits,
            EGL14.EGL_GREEN_SIZE, bits,
            EGL14.EGL_BLUE_SIZE, bits,
            EGL14.EGL_ALPHA_SIZE, (64 - 3 * bits) % 32,
            EGL_COLOR_COMPONENT_TYPE_EXT, if(bits == 16) EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT else EGL_COLOR_COMPONENT_TYPE_FIXED_EXT,
            EGL14.EGL_NONE
        )
        val configs = Array<EGLConfig?>(1) { null }
        val configCount = IntArray(1)
        if (!EGL14.eglChooseConfig(eglDisplay, configAttr, 0, configs, 0, 1, configCount, 0))
            throw RuntimeException("Fail to choose config")
        val config = configs[0]!!

        val surfaceAttr = intArrayOf(
            EGL15.EGL_GL_COLORSPACE, colorspace,
            EGL14.EGL_NONE
        )
        eglSurface = EGL14.eglCreateWindowSurface(eglDisplay, config, surface, surfaceAttr, 0)

        val contextAttr = intArrayOf(
            EGL15.EGL_CONTEXT_MAJOR_VERSION, 3,
            EGL15.EGL_CONTEXT_MINOR_VERSION, 1,
            EGL14.EGL_NONE
        )
        eglContext = EGL14.eglCreateContext(eglDisplay, config, EGL14.EGL_NO_CONTEXT, contextAttr, 0)

        EGL14.eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)
        Log.d("SORAYUKI", "surface size is ${binding.surfaceView.width}x${binding.surfaceView.height}")
        GLES31.glViewport(0, 0, binding.surfaceView.width, binding.surfaceView.height)
        GLES31.glClearColor(red, green, blue, 1.0f)
        GLES31.glClear(GLES31.GL_COLOR_BUFFER_BIT)
        EGL14.eglSwapBuffers(eglDisplay, eglSurface)
        EGL14.eglMakeCurrent(eglDisplay, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT)
    }

    fun drawFrame(colorspace: Int) {

    }
}
