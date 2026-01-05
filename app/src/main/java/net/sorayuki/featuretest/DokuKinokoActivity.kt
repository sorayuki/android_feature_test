package net.sorayuki.featuretest

import android.os.Bundle
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.Button
import android.widget.TextView
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import java.util.concurrent.atomic.AtomicBoolean

class DokuKinokoActivity : AppCompatActivity(), SurfaceHolder.Callback {

    private var renderThread: Thread? = null
    private val isRunning = AtomicBoolean(false)
    private val isSurfaceCreated = AtomicBoolean(false)
    
    @Volatile private var surfaceWidth = 0
    @Volatile private var surfaceHeight = 0
    @Volatile private var sizeChanged = false
    
    private val lastFps = DoubleArray(3) { 0.0 }
    private lateinit var fpsTextView: TextView

    companion object {
        init {
            System.loadLibrary("featuretest")
        }
    }

    external fun nativeInit(surface: Surface)
    external fun nativeResize(width: Int, height: Int)
    external fun nativeRender()
    external fun nativeDestroy()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_doku_kinoko)
        
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        val surfaceView = findViewById<SurfaceView>(R.id.KinokoSurface)
        surfaceView.holder.addCallback(this)
        
        fpsTextView = findViewById(R.id.KinokoFramerateTextView)

        findViewById<Button>(R.id.StartKinoko).setOnClickListener {
            startRendering()
        }
    }

    override fun onPause() {
        super.onPause()
        stopRendering()
    }

    private fun startRendering() {
        if (isRunning.get() || !isSurfaceCreated.get()) return

        isRunning.set(true)
        val surface = findViewById<SurfaceView>(R.id.KinokoSurface).holder.surface
        
        renderThread = Thread {
            nativeInit(surface)
            
            // Force initial resize
            nativeResize(surfaceWidth, surfaceHeight)
            sizeChanged = false

            var lastTime = System.nanoTime()

            while (isRunning.get()) {
                if (sizeChanged) {
                    nativeResize(surfaceWidth, surfaceHeight)
                    sizeChanged = false
                }

                nativeRender()

                val now = System.nanoTime()
                val diff = now - lastTime
                lastTime = now
                
                if (diff > 0) {
                    val fps = 1_000_000_000.0 / diff
                    updateFps(fps)
                }
            }
            nativeDestroy()
        }
        renderThread?.start()
    }

    private fun stopRendering() {
        if (!isRunning.get()) return
        isRunning.set(false)
        try {
            renderThread?.join(500)
        } catch (e: InterruptedException) {
            e.printStackTrace()
        }
        renderThread = null
    }

    private fun updateFps(currentFps: Double) {
        // Shift values: [0] is newest
        lastFps[2] = lastFps[1]
        lastFps[1] = lastFps[0]
        lastFps[0] = currentFps

        // Weights: 4, 2, 1
        val weightedFps = (lastFps[0] * 4.0 + lastFps[1] * 2.0 + lastFps[2] * 1.0) / 7.0

        runOnUiThread {
            fpsTextView.text = String.format("FPS: %.2f", weightedFps)
        }
    }

    // SurfaceHolder.Callback methods
    override fun surfaceCreated(holder: SurfaceHolder) {
        isSurfaceCreated.set(true)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        surfaceWidth = width
        surfaceHeight = height
        sizeChanged = true
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        isSurfaceCreated.set(false)
        stopRendering()
    }
}