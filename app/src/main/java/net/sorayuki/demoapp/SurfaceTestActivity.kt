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
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer
import kotlin.div

class SurfaceTestActivity : AppCompatActivity() {
    
    lateinit var bgThread: HandlerThread
    lateinit var bgHandler: Handler

    lateinit var surface: Surface

    lateinit var binding: ActivitySurfacetestBinding

    val EGL_GL_COLORSPACE_DISPLAY_P3_EXT = 0x3363
    val EGL_GL_COLORSPACE_SCRGB_LINEAR_EXT = 0x3350
    val EGL_GL_COLORSPACE_BT2020_PQ_EXT = 0x3340
    val EGL_GL_COLORSPACE_BT2020_HLG_EXT = 0x3540

    val EGL_COLOR_COMPONENT_TYPE_EXT = 0x3339
    val EGL_COLOR_COMPONENT_TYPE_FIXED_EXT = 0x333A
    val EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT = 0x333B

    fun LogGlError(operation: String) {
        val err = GLES31.glGetError()
        if (err != GLES31.GL_NO_ERROR) {
            Log.e("SORAYUKI", "$operation ${String.format("%x", err)}")
        }
    }

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

        binding.btnSRGB.setOnClickListener {
            bgHandler.post {
                draw(8, EGL15.EGL_GL_COLORSPACE_SRGB, true)
                draw(8, EGL15.EGL_GL_COLORSPACE_SRGB, false)
            }
        }
        binding.btnP3.setOnClickListener {
            bgHandler.post {
                draw(8, EGL15.EGL_GL_COLORSPACE_SRGB, true)
                draw(8, EGL_GL_COLORSPACE_DISPLAY_P3_EXT, false)
            }
        }
        binding.btnScRGB.setOnClickListener {
            bgHandler.post {
                draw(8, EGL15.EGL_GL_COLORSPACE_SRGB, true)
                draw(16, EGL_GL_COLORSPACE_SCRGB_LINEAR_EXT, false)
            }
        }
        binding.btnHLG.setOnClickListener {
            bgHandler.post {
                draw(8, EGL15.EGL_GL_COLORSPACE_SRGB, true)
                draw(10, EGL_GL_COLORSPACE_BT2020_HLG_EXT, false)
            }
        }
        binding.btnPQ.setOnClickListener {
            bgHandler.post {
                draw(8, EGL15.EGL_GL_COLORSPACE_SRGB, true)
                draw(10, EGL_GL_COLORSPACE_BT2020_PQ_EXT, false)
            }
        }
    }

    var eglDisplay: EGLDisplay = EGL14.EGL_NO_DISPLAY
    var eglSurface: EGLSurface = EGL14.EGL_NO_SURFACE
    var eglContext: EGLContext = EGL14.EGL_NO_CONTEXT

    fun draw(bits: Int, colorspace: Int, isClear: Boolean) {
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
        GLES31.glClearColor(0.0f, 0.0f, 0.0f, 1.0f)
        GLES31.glClear(GLES31.GL_COLOR_BUFFER_BIT)
        if (!isClear)
            drawFrame(colorspace)
        EGL14.eglSwapBuffers(eglDisplay, eglSurface)
        EGL14.eglMakeCurrent(eglDisplay, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT)
    }

    fun drawFrame(colorspace: Int) {
        val frameWidth = 1080
        val frameHeight = 1920
        val chromaWidth = maxOf(1, frameWidth / 2)
        val chromaHeight = maxOf(1, frameHeight / 2)

        val textures = IntArray(2)
        GLES31.glGenTextures(2, textures, 0)
        val vertexShaderSource = """#version 310 es
        layout(location = 0) in vec2 aPosition;
        layout(location = 1) in vec2 aTexCoord;
        out vec2 vTexCoord;
        void main() {
            vTexCoord = aTexCoord;
            gl_Position = vec4(aPosition, 0.0, 1.0);
        }
    """.trimIndent()
        val fragmentShaderSource = """#version 310 es
        precision highp float;
        
        in vec2 vTexCoord;
        layout(location = 0) out vec4 fragColor;
        
        uniform highp usampler2D uTexY;
        uniform highp usampler2D uTexUV;
        uniform int uTargetSpace;
        
        const float HLG_A = 0.17883277;
        const float HLG_B = 0.28466892;
        const float HLG_C = 0.55991073;
        const float HLG_PEAK_LUMINANCE = 1000.0;  // HLG 峰值亮度 (nits)
        const float SDR_PEAK_LUMINANCE = 100.0;   // SDR 峰值亮度 (nits)
        const float EPS = 1e-5;
        
        // BT.2020 RGB to XYZ (D65) - 列主序 (column-major)
        const mat3 BT2020_TO_XYZ = mat3(
            0.6369580483012914, 0.2627002120112671, 0.0,
            0.14461690358620832, 0.6779980715188708, 0.028072693049087428,
            0.1688809751641721, 0.05930171646986196, 1.0609850577118231
        );
        // XYZ to sRGB (D65) - 列主序 (column-major)
        const mat3 XYZ_TO_SRGB = mat3(
            3.2404542, -0.9692660, 0.0556434,
            -1.5371385, 1.8760108, -0.2040259,
            -0.4985314, 0.0415560, 1.0572252
        );
        // XYZ to Display P3 (D65) - 列主序 (column-major)
        const mat3 XYZ_TO_P3 = mat3(
            2.4934969119, -0.8294889695, 0.0358458302,
            -0.9313836179, 1.7626640603, -0.0761723893,
            -0.4027107845, 0.0236246858, 0.9568845240
        );
        
        float inverseHLG(float value) {
            value = clamp(value, 0.0, 1.0);
            if (value <= 0.5) {
                return (value * value) / 3.0;
            }
            return (exp((value - HLG_C) / HLG_A) + HLG_B) / 12.0;
        }
        
        vec3 inverseHLGColor(vec3 color) {
            return vec3(
                inverseHLG(color.r),
                inverseHLG(color.g),
                inverseHLG(color.b)
            );
        }
        
        float srgbOetf(float linear) {
            linear = clamp(linear, 0.0, 1.0);
            if (linear <= 0.0031308) {
                return linear * 12.92;
            }
            return 1.055 * pow(linear, 1.0 / 2.4) - 0.055;
        }
        
        float hlgOetf(float linear) {
            if (linear <= 1.0 / 12.0) {
                return sqrt(3.0 * linear);
            }
            return HLG_A * log(12.0 * linear - HLG_B) + HLG_C;
        }
        
        float pqOetf(float linear) {
            const float m1 = 0.1593017578125;
            const float m2 = 78.84375;
            const float c1 = 0.8359375;
            const float c2 = 18.8515625;
            const float c3 = 18.6875;
            float powVal = pow(max(linear, 0.0), m1);
            float numerator = c1 + c2 * powVal;
            float denominator = 1.0 + c3 * powVal;
            return pow(numerator / denominator, m2);
        }
        
        vec3 yuvToHLGBT2020(vec3 yuv) {
            float y = yuv.x;
            float cb = yuv.y - 0.5;
            float cr = yuv.z - 0.5;
            
            return vec3(
                y + 1.4746 * cr,
                y - 0.16455 * cb - 0.57135 * cr,
                y + 1.8814 * cb
            );
        }
        
        vec3 toneMapBT2390(vec3 linear) {
            // BT.2020 亮度权重
            const vec3 BT2020_LUMA = vec3(0.2627, 0.6780, 0.0593);
            
            // 计算场景参考亮度
            // 输入 linear 的单位是 1/1000 nit，即 linear = 1.0 对应 1000 nits
            float srcLuminance = dot(linear, BT2020_LUMA);
            
            if (srcLuminance <= EPS) {
                return linear;
            }
            
            // srcLuminance 已经是以 1000 nits 为单位，直接转换为 nits
            float srcLuminanceNits = srcLuminance * 1000.0;
            
            // BT.2390 EETF - 色调映射到 SDR (100 nits)
            float targetLuminanceNits;
            
            if (srcLuminanceNits <= SDR_PEAK_LUMINANCE) {
                // 低亮度区域：线性传递
                targetLuminanceNits = srcLuminanceNits;
            } else {
                // 高亮度区域：使用软压缩曲线
                // 简化的 BT.2390 EETF 曲线
                float maxLuminanceNits = HLG_PEAK_LUMINANCE;  // 1000 nits
                
                // 归一化到 [0, 1] 范围
                float pNorm = (srcLuminanceNits - SDR_PEAK_LUMINANCE) / (maxLuminanceNits - SDR_PEAK_LUMINANCE);
                pNorm = clamp(pNorm, 0.0, 1.0);
                
                // 软膝点压缩：使用二次函数进行软裁剪
                // 从 100 nits 平滑过渡，避免突变
                float compressed = 1.0 - pNorm * pNorm;
                
                // 映射到 SDR 范围的剩余空间 (实际上很小，主要用于高光)
                float headroom = 10.0;  // 给高光留 10 nits 的空间
                targetLuminanceNits = SDR_PEAK_LUMINANCE - headroom * (1.0 - compressed);
            }
            
            // 计算缩放比例，转换回 1/1000 nit 单位
            float targetLuminance = targetLuminanceNits / 1000.0;
            float scale = targetLuminance / max(EPS, srcLuminance);
            
            // 应用缩放，保持色度
            return linear * scale;
        }
        
        vec3 convertToTarget(vec3 hlg) {
            if (uTargetSpace == 0) { // SRGB
                vec3 linear = inverseHLGColor(hlg);
                vec3 xyz = BT2020_TO_XYZ * linear;
                vec3 srgbLinear = XYZ_TO_SRGB * xyz;
                return vec3(
                    srgbOetf(srgbLinear.r),
                    srgbOetf(srgbLinear.g),
                    srgbOetf(srgbLinear.b)
                );
            } else if (uTargetSpace == 1) { // P3
                vec3 linear = inverseHLGColor(hlg);
                vec3 xyz = BT2020_TO_XYZ * linear;
                vec3 p3Linear = XYZ_TO_P3 * xyz;
                return vec3(
                    srgbOetf(p3Linear.r),
                    srgbOetf(p3Linear.g),
                    srgbOetf(p3Linear.b)
                );
            } else if (uTargetSpace == 2) { // Linear
                vec3 xyz = BT2020_TO_XYZ * inverseHLGColor(hlg);
                return XYZ_TO_SRGB * xyz;
            } else if (uTargetSpace == 3) { // HLG
                return hlg;
            } else { // PQ
                vec3 linear = inverseHLGColor(hlg);
                return vec3(
                    pqOetf(linear.r / 10.0),
                    pqOetf(linear.g / 10.0),
                    pqOetf(linear.b / 10.0)
                );
            }
        }
        
        void main() {
            float y = float(texture(uTexY, vTexCoord).r - 16u * 256u) / float(235 * 256 - 16 * 256);
            vec2 uv = vec2(texture(uTexUV, vTexCoord).rg - 16u * 256u) / float(240 * 256 - 16 * 256);
            vec3 yuv = vec3(y, uv.r, uv.g);
            vec3 hlg = yuvToHLGBT2020(yuv);
            vec3 color = convertToTarget(hlg);
            fragColor = vec4(color, 1.0);
        }
    """.trimIndent()

        var vertexShader = 0
        var fragmentShader = 0
        var program = 0
        val quadBuffer = createFullScreenQuad()
        val targetSpaceIndex = when (colorspace) {
            EGL_GL_COLORSPACE_DISPLAY_P3_EXT -> 1
            EGL_GL_COLORSPACE_SCRGB_LINEAR_EXT -> 2
            EGL_GL_COLORSPACE_BT2020_HLG_EXT -> 3
            EGL_GL_COLORSPACE_BT2020_PQ_EXT -> 4
            else -> 0
        }

        try {
            vertexShader = compileShader(GLES31.GL_VERTEX_SHADER, vertexShaderSource)
            fragmentShader = compileShader(GLES31.GL_FRAGMENT_SHADER, fragmentShaderSource)
            program = linkProgram(vertexShader, fragmentShader)
            GLES31.glDeleteShader(vertexShader)
            GLES31.glDeleteShader(fragmentShader)
            vertexShader = 0
            fragmentShader = 0

            val rawp010 = ByteBuffer.wrap(resources.openRawResource(R.raw.aqua).readBytes())
            rawp010.order(ByteOrder.nativeOrder())
            rawp010.position(0)
            uploadPlaneTexture(textures[0], frameWidth, frameHeight, rawp010)
            LogGlError("upload y texture")
            rawp010.position(frameWidth * frameHeight * 2)
            uploadUVPlaneTexture(textures[1], chromaWidth, chromaHeight, rawp010)
            LogGlError("upload uv texture")

            GLES31.glUseProgram(program)
            GLES31.glUniform1i(GLES31.glGetUniformLocation(program, "uTexY"), 0)
            GLES31.glUniform1i(GLES31.glGetUniformLocation(program, "uTexUV"), 1)
            GLES31.glUniform1i(GLES31.glGetUniformLocation(program, "uTargetSpace"), targetSpaceIndex)

            quadBuffer.position(0)
            GLES31.glEnableVertexAttribArray(0)
            GLES31.glVertexAttribPointer(0, 2, GLES31.GL_FLOAT, false, 4 * 4, quadBuffer)
            quadBuffer.position(2)
            GLES31.glEnableVertexAttribArray(1)
            GLES31.glVertexAttribPointer(1, 2, GLES31.GL_FLOAT, false, 4 * 4, quadBuffer)
            quadBuffer.position(0)

            for (i in textures.indices) {
                GLES31.glActiveTexture(GLES31.GL_TEXTURE0 + i)
                GLES31.glBindTexture(GLES31.GL_TEXTURE_2D, textures[i])
                LogGlError("glBindTexture")
            }

            GLES31.glViewport(0, 0, binding.surfaceView.width, binding.surfaceView.height)
            GLES31.glDrawArrays(GLES31.GL_TRIANGLE_STRIP, 0, 4)
            LogGlError("glDrawArray")
            GLES31.glDisableVertexAttribArray(0)
            GLES31.glDisableVertexAttribArray(1)
            GLES31.glUseProgram(0)
            for (i in textures.indices) {
                GLES31.glActiveTexture(GLES31.GL_TEXTURE0 + i)
                GLES31.glBindTexture(GLES31.GL_TEXTURE_2D, 0)
            }
        } finally {
            if (program != 0) {
                GLES31.glDeleteProgram(program)
            }
            if (vertexShader != 0) {
                GLES31.glDeleteShader(vertexShader)
            }
            if (fragmentShader != 0) {
                GLES31.glDeleteShader(fragmentShader)
            }
            if (textures.any { it != 0 }) {
                GLES31.glDeleteTextures(textures.size, textures, 0)
            }
        }
    }

    private fun uploadPlaneTexture(texture: Int, width: Int, height: Int, data: ByteBuffer) {
        GLES31.glBindTexture(GLES31.GL_TEXTURE_2D, texture)
        GLES31.glTexParameteri(GLES31.GL_TEXTURE_2D, GLES31.GL_TEXTURE_MIN_FILTER, GLES31.GL_NEAREST)
        GLES31.glTexParameteri(GLES31.GL_TEXTURE_2D, GLES31.GL_TEXTURE_MAG_FILTER, GLES31.GL_NEAREST)
        GLES31.glTexParameteri(GLES31.GL_TEXTURE_2D, GLES31.GL_TEXTURE_WRAP_S, GLES31.GL_CLAMP_TO_EDGE)
        GLES31.glTexParameteri(GLES31.GL_TEXTURE_2D, GLES31.GL_TEXTURE_WRAP_T, GLES31.GL_CLAMP_TO_EDGE)
        GLES31.glTexImage2D(GLES31.GL_TEXTURE_2D, 0, GLES31.GL_R16UI, width, height, 0, GLES31.GL_RED_INTEGER, GLES31.GL_UNSIGNED_SHORT, data)
        GLES31.glBindTexture(GLES31.GL_TEXTURE_2D, 0)
    }

    private fun uploadUVPlaneTexture(texture: Int, width: Int, height: Int, data: ByteBuffer) {
        GLES31.glBindTexture(GLES31.GL_TEXTURE_2D, texture)
        GLES31.glTexParameteri(GLES31.GL_TEXTURE_2D, GLES31.GL_TEXTURE_MIN_FILTER, GLES31.GL_NEAREST)
        GLES31.glTexParameteri(GLES31.GL_TEXTURE_2D, GLES31.GL_TEXTURE_MAG_FILTER, GLES31.GL_NEAREST)
        GLES31.glTexParameteri(GLES31.GL_TEXTURE_2D, GLES31.GL_TEXTURE_WRAP_S, GLES31.GL_CLAMP_TO_EDGE)
        GLES31.glTexParameteri(GLES31.GL_TEXTURE_2D, GLES31.GL_TEXTURE_WRAP_T, GLES31.GL_CLAMP_TO_EDGE)
        GLES31.glTexImage2D(GLES31.GL_TEXTURE_2D, 0, GLES31.GL_RG16UI, width, height, 0, GLES31.GL_RG_INTEGER, GLES31.GL_UNSIGNED_SHORT, data)
        GLES31.glBindTexture(GLES31.GL_TEXTURE_2D, 0)
    }

    private fun createFullScreenQuad(): FloatBuffer {
        return ByteBuffer
            .allocateDirect(4 * 4 * 4)
            .order(ByteOrder.nativeOrder())
            .asFloatBuffer()
            .apply {
                put(
                    floatArrayOf(
                        -1f, -1f, 0f, 1f,
                        1f, -1f, 1f, 1f,
                        -1f, 1f, 0f, 0f,
                        1f, 1f, 1f, 0f
                    )
                )
                position(0)
            }
    }

    private fun compileShader(type: Int, source: String): Int {
        val shader = GLES31.glCreateShader(type)
        if (shader == 0) {
            throw RuntimeException("Failed to create shader")
        }
        GLES31.glShaderSource(shader, source)
        GLES31.glCompileShader(shader)
        val status = IntArray(1)
        GLES31.glGetShaderiv(shader, GLES31.GL_COMPILE_STATUS, status, 0)
        if (status[0] == 0) {
            val log = GLES31.glGetShaderInfoLog(shader)
            GLES31.glDeleteShader(shader)
            throw RuntimeException("Shader compile failed: $log")
        }
        return shader
    }

    private fun linkProgram(vertexShader: Int, fragmentShader: Int): Int {
        val program = GLES31.glCreateProgram()
        if (program == 0) {
            throw RuntimeException("Failed to create program")
        }
        GLES31.glAttachShader(program, vertexShader)
        GLES31.glAttachShader(program, fragmentShader)
        GLES31.glLinkProgram(program)
        val status = IntArray(1)
        GLES31.glGetProgramiv(program, GLES31.GL_LINK_STATUS, status, 0)
        if (status[0] == 0) {
            val log = GLES31.glGetProgramInfoLog(program)
            GLES31.glDeleteProgram(program)
            throw RuntimeException("Program link failed: $log")
        }
        return program
    }
}
