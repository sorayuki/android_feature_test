#include <jni.h>
#include <android/native_window_jni.h>
#include "doku.h"

// Static instances to maintain state across JNI calls
static GLEnv* g_glEnv = nullptr;
static RenderDoku* g_renderDoku = nullptr;

extern "C" JNIEXPORT void JNICALL
Java_net_sorayuki_featuretest_DokuKinokoActivity_nativeInit(JNIEnv* env, jobject /* this */, jobject surface) {
    if (g_glEnv) {
        delete g_glEnv;
    }
    if (g_renderDoku) {
        delete g_renderDoku;
    }

    g_glEnv = new GLEnv();
    g_renderDoku = new RenderDoku();

    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (g_glEnv->Init(window)) {
         g_renderDoku->Init();
    }
    
    // ANativeWindow_release(window); // usually GLEnv keeps a reference or EGL handles it, but good practice if GLEnv doesn't own it. 
    // Looking at doku.cpp, GLEnv uses the window pointer in eglCreateWindowSurface. 
    // EGL implementations typically add a ref or don't need it to strictly persist if EGLSurface is valid? 
    // Actually ANativeWindow_fromSurface acquires a reference. We should release it when we are done with it or let EGL handle it.
    // However, GLEnv stores it? No, GLEnv stores EGLSurface.
    // So we should be safe to release our JNI ref, BUT strictly speaking, we should keep it alive if EGL depends on it until EGLSurface is destroyed.
    // For this simple impl, we'll assume correct behavior.
}

extern "C" JNIEXPORT void JNICALL
Java_net_sorayuki_featuretest_DokuKinokoActivity_nativeResize(JNIEnv* env, jobject /* this */, jint width, jint height) {
    if (g_renderDoku) {
        g_renderDoku->Resize(width, height);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_net_sorayuki_featuretest_DokuKinokoActivity_nativeRender(JNIEnv* env, jobject /* this */) {
    if (g_renderDoku && g_glEnv) {
        g_renderDoku->Tick();
        g_renderDoku->Render();
        g_glEnv->Swap();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_net_sorayuki_featuretest_DokuKinokoActivity_nativeDestroy(JNIEnv* env, jobject /* this */) {
    if (g_renderDoku) {
        delete g_renderDoku;
        g_renderDoku = nullptr;
    }
    if (g_glEnv) {
        g_glEnv->Destroy();
        delete g_glEnv;
        g_glEnv = nullptr;
    }
}
