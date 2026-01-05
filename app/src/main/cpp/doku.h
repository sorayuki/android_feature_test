#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <iostream>

class GLEnv {
public:
    GLEnv();
    ~GLEnv();

    bool Init(void* window); // Changed from ANativeWindow* to void* for cross-platform compatibility
    void Swap();
    void Destroy();

private:
    EGLDisplay m_display;
    EGLContext m_context;
    EGLSurface m_surface;
};

class RenderDoku {
public:
    RenderDoku();
    ~RenderDoku();

    void Init();
    void Resize(int width, int height);
    void Tick();
    void Render();

private:
    GLuint CreateShader(GLenum type, const char* source);
    GLuint CreateProgram(const char* vertexSource, const char* fragmentSource);
    
    GLuint m_program;
    GLuint m_vbo;

    // Uniform locations
    GLint m_uRight;
    GLint m_uForward;
    GLint m_uUp;
    GLint m_uOrigin;
    GLint m_uX;
    GLint m_uY;
    GLint m_uLen;

    // Attribute locations
    GLint m_aPosition;

    // State variables from JS
    float cx, cy;
    float len = 1.6f;
    float ang1 = 2.8f;
    float ang2 = 0.4f;
    float cenx = 0.0f;
    float ceny = 0.0f;
    float cenz = 0.0f;

    // Viewport centering
    int m_offsetX = 0;
    int m_offsetY = 0;
    int m_viewportSize = 0;
    int m_windowWidth = 0;
    int m_windowHeight = 0;

    // Offscreen Rendering
    GLuint m_fbo = 0;
    GLuint m_fboTexture = 0;
    const int FBO_SIZE = 1024;
};
