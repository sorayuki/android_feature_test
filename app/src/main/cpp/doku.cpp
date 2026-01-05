#include "doku.h"
#include "doku.h"
#include <vector>

#ifdef __ANDROID__
    #include <android/log.h>
    #include <android/native_window.h>
    #define LOG_TAG "DOKU_RENDER"
    #define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
    #include <cstdio>
    #define LOGE(...) do { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#endif


// ================= GLEnv =================

GLEnv::GLEnv() : m_display(EGL_NO_DISPLAY), m_context(EGL_NO_CONTEXT), m_surface(EGL_NO_SURFACE) {}

GLEnv::~GLEnv() {
    Destroy();
}

bool GLEnv::Init(void* window) {
    m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_display == EGL_NO_DISPLAY) return false;

    if (!eglInitialize(m_display, nullptr, nullptr)) return false;

    const EGLint configAttribs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_SAMPLE_BUFFERS, 0,
        EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(m_display, configAttribs, &config, 1, &numConfigs)) return false;

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 1,
        EGL_NONE
    };

    m_context = eglCreateContext(m_display, config, EGL_NO_CONTEXT, contextAttribs);
    if (m_context == EGL_NO_CONTEXT) return false;

    #ifdef __ANDROID__
        m_surface = eglCreateWindowSurface(m_display, config, (ANativeWindow*)window, nullptr);
    #else
        m_surface = eglCreateWindowSurface(m_display, config, (EGLNativeWindowType)window, nullptr);
    #endif

    if (m_surface == EGL_NO_SURFACE) return false;

    if (!eglMakeCurrent(m_display, m_surface, m_surface, m_context)) return false;

    // Disable VSync for performance testing
    eglSwapInterval(m_display, 0);

    return true;
}

void GLEnv::Swap() {
    eglSwapBuffers(m_display, m_surface);
}

void GLEnv::Destroy() {
    if (m_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (m_context != EGL_NO_CONTEXT) eglDestroyContext(m_display, m_context);
        if (m_surface != EGL_NO_SURFACE) eglDestroySurface(m_display, m_surface);
        eglTerminate(m_display);
    }
    m_display = EGL_NO_DISPLAY;
    m_context = EGL_NO_CONTEXT;
    m_surface = EGL_NO_SURFACE;
}

// ================= RenderDoku =================

RenderDoku::RenderDoku() : m_program(0), m_vbo(0) {}
RenderDoku::~RenderDoku() {
    if (m_program) glDeleteProgram(m_program);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
    if (m_fboTexture) glDeleteTextures(1, &m_fboTexture);
}

const char* VERT_SHADER = R"(#version 310 es
in vec4 position;
out vec3 dir, localdir;
uniform vec3 right, forward, up, origin;
uniform float x,y;
void main() {
   gl_Position = position; 
   dir = forward + right * position.x*x + up * position.y*y;
   localdir.x = position.x*x;
   localdir.y = position.y*y;
   localdir.z = -1.0;
}
)";

const char* FRAG_SHADER_PREFIX = R"(#version 310 es
#define PI 3.14159265358979324
#define M_L 0.3819660113
#define M_R 0.6180339887
#define MAXR 8
#define SOLVER 8
precision highp float;
out highp vec4 fragColor;
// Forward declaration of kernal
float kernal(vec3 ver);
uniform vec3 right, forward, up, origin;
in vec3 dir, localdir;
uniform float len;
// Global variables for main loop to avoid stack overflow issues in some compilers (not strictly needed for GLSL ES but keeping close to source)
// However, locals are better. Let's keep structure similar.

)";

const char* KERNEL_SOURCE = R"(
float kernal(vec3 ver){
   vec3 a;
   float b,c,d,e;
   a=ver;
   for(int i=0;i<5;i++){
       b=length(a);
       c=atan(a.y,a.x)*8.0;
       e=1.0/b;
       d=acos(a.z/b)*8.0;
       b=pow(b,8.0);
       a=vec3(b*sin(d)*cos(c),b*sin(d)*sin(c),b*cos(d))+ver;
       if(b>6.0){
           break;
       }
   }
   return 4.0-a.x*a.x-a.y*a.y-a.z*a.z;
}
)";

const char* FRAG_SHADER_MAIN = R"(
void main() {
   vec3 color;
   color.r=0.0;
   color.g=0.0;
   color.b=0.0;
   int sign=0;
   const float step = 0.002;
   
   float v1 = kernal(origin + dir * (step*len));
   float v2 = kernal(origin);
   float v;
   
   float r1, r2, r3, r4, m1, m2, m3;
   
   for (int k = 2; k < 1002; k++) {
      vec3 ver = origin + dir * (step*len*float(k));
      v = kernal(ver);
      if (v > 0.0 && v1 < 0.0) {
         r1 = step * len*float(k - 1);
         r2 = step * len*float(k);
         m1 = kernal(origin + dir * r1);
         m2 = kernal(origin + dir * r2);
         for (int l = 0; l < SOLVER; l++) {
            r3 = r1 * 0.5 + r2 * 0.5;
            m3 = kernal(origin + dir * r3);
            if (m3 > 0.0) {
               r2 = r3;
               m2 = m3;
            }
            else {
               r1 = r3;
               m1 = m3;
            }
         }
         if (r3 < 2.0 * len) {
               sign=1;
            break;
         }
      }
      if (v < v1&&v1>v2&&v1 < 0.0 && (v1*2.0 > v || v1 * 2.0 > v2)) {
         r1 = step * len*float(k - 2);
         r2 = step * len*(float(k) - 2.0 + 2.0*M_L);
         r3 = step * len*(float(k) - 2.0 + 2.0*M_R);
         r4 = step * len*float(k);
         m2 = kernal(origin + dir * r2);
         m3 = kernal(origin + dir * r3);
         for (int l = 0; l < MAXR; l++) {
            if (m2 > m3) {
               r4 = r3;
               r3 = r2;
               r2 = r4 * M_L + r1 * M_R;
               m3 = m2;
               m2 = kernal(origin + dir * r2);
            }
            else {
               r1 = r2;
               r2 = r3;
               r3 = r4 * M_R + r1 * M_L;
               m2 = m3;
               m3 = kernal(origin + dir * r3);
            }
         }
         if (m2 > 0.0) {
            r1 = step * len*float(k - 2);
            // r2 = r2; // optimization
            m1 = kernal(origin + dir * r1);
            m2 = kernal(origin + dir * r2);
            for (int l = 0; l < SOLVER; l++) {
               r3 = r1 * 0.5 + r2 * 0.5;
               m3 = kernal(origin + dir * r3);
               if (m3 > 0.0) {
                  r2 = r3;
                  m2 = m3;
               }
               else {
                  r1 = r3;
                  m1 = m3;
               }
            }
            if (r3 < 2.0 * len&&r3> step*len) {
                   sign=1;
               break;
            }
         }
         else if (m3 > 0.0) {
            r1 = step * len*float(k - 2);
            r2 = r3;
            m1 = kernal(origin + dir * r1);
            m2 = kernal(origin + dir * r2);
            for (int l = 0; l < SOLVER; l++) {
               r3 = r1 * 0.5 + r2 * 0.5;
               m3 = kernal(origin + dir * r3);
               if (m3 > 0.0) {
                  r2 = r3;
                  m2 = m3;
               }
               else {
                  r1 = r3;
                  m1 = m3;
               }
            }
            if (r3 < 2.0 * len&&r3> step*len) {
                   sign=1;
               break;
            }
         }
      }
      v2 = v1;
      v1 = v;
   }
   if (sign==1) {
      vec3 ver = origin + dir*r3 ;
       float r1_sq=ver.x*ver.x+ver.y*ver.y+ver.z*ver.z;
      vec3 n;
      n.x = kernal(ver - right * (r3*0.00025)) - kernal(ver + right * (r3*0.00025));
      n.y = kernal(ver - up * (r3*0.00025)) - kernal(ver + up * (r3*0.00025));
      n.z = kernal(ver + forward * (r3*0.00025)) - kernal(ver - forward * (r3*0.00025));
      r3 = n.x*n.x+n.y*n.y+n.z*n.z;
      n = n * (1.0 / sqrt(r3));
      ver = localdir;
      r3 = ver.x*ver.x+ver.y*ver.y+ver.z*ver.z;
      ver = ver * (1.0 / sqrt(r3));
      vec3 reflect = n * (-2.0*dot(ver, n)) + ver;
      r3 = reflect.x*0.276+reflect.y*0.920+reflect.z*0.276;
      r4 = n.x*0.276+n.y*0.920+n.z*0.276;
      r3 = max(0.0,r3);
      r3 = r3 * r3*r3*r3;
      r3 = r3 * 0.45 + r4 * 0.25 + 0.3;
      n.x = sin(r1_sq*10.0)*0.5+0.5;
      n.y = sin(r1_sq*10.0+2.05)*0.5+0.5;
      n.z = sin(r1_sq*10.0-2.05)*0.5+0.5;
      color = n*r3;
   }
   fragColor = vec4(color.x, color.y, color.z, 1.0);
}
)";

GLuint RenderDoku::CreateShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            std::vector<char> infoLog(infoLen);
            glGetShaderInfoLog(shader, infoLen, nullptr, infoLog.data());
            LOGE("Error compiling shader:\n%s", infoLog.data());
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint RenderDoku::CreateProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = CreateShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = CreateShader(GL_FRAGMENT_SHADER, fragmentSource);

    if (!vertexShader || !fragmentShader) return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint infoLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            std::vector<char> infoLog(infoLen);
            glGetProgramInfoLog(program, infoLen, nullptr, infoLog.data());
            LOGE("Error linking program:\n%s", infoLog.data());
        }
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

void RenderDoku::Init() {
    std::string fragSource = std::string(FRAG_SHADER_PREFIX) + KERNEL_SOURCE + FRAG_SHADER_MAIN;
    m_program = CreateProgram(VERT_SHADER, fragSource.c_str());

    if (!m_program) {
        LOGE("Failed to create program");
        return;
    }

    glUseProgram(m_program);

    m_uRight = glGetUniformLocation(m_program, "right");
    m_uForward = glGetUniformLocation(m_program, "forward");
    m_uUp = glGetUniformLocation(m_program, "up");
    m_uOrigin = glGetUniformLocation(m_program, "origin");
    m_uX = glGetUniformLocation(m_program, "x");
    m_uY = glGetUniformLocation(m_program, "y");
    m_uLen = glGetUniformLocation(m_program, "len");
    m_aPosition = glGetAttribLocation(m_program, "position");

    float positions[] = {
        -1.0f, -1.0f, 0.0f, 
         1.0f, -1.0f, 0.0f, 
         1.0f,  1.0f, 0.0f, 
        -1.0f, -1.0f, 0.0f, 
         1.0f,  1.0f, 0.0f, 
        -1.0f,  1.0f, 0.0f
    };

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);
    
    glVertexAttribPointer(m_aPosition, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(m_aPosition);

    // Initialize FBO
    GLint oldDrawFBO, oldReadFBO;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFBO);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFBO);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_fboTexture);
    glBindTexture(GL_TEXTURE_2D, m_fboTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FBO_SIZE, FBO_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fboTexture, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("Framebuffer not complete: %x", status);
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldDrawFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, oldReadFBO);
}

void RenderDoku::Resize(int width, int height) {
    m_windowWidth = width;
    m_windowHeight = height;
    int size = (std::min)(width, height);
    m_viewportSize = size;
    m_offsetX = (width - size) / 2;
    m_offsetY = (height - size) / 2;

    cx = (float)size;
    cy = (float)size;
}

void RenderDoku::Tick() {
    ang1 += 0.01f;
}

void RenderDoku::Render() {
    GLint oldDrawFBO, oldReadFBO;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDrawFBO);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldReadFBO);

    // Render to FBO
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, FBO_SIZE, FBO_SIZE);
    
    glUseProgram(m_program);

    // Uniform setting logic from draw()
    // gl.uniform1f(glx, cx * 2.0 / (cx + cy)); -> In our case cx=cy, so 2*cx / 2*cx = 1.0
    // But let's keep the formula generic or simplified since we enforce square viewport
    float ratioX = 1.0f; // cx * 2.0 / (cx + cy); since cx==cy, this is 1.0.
    float ratioY = 1.0f; // cy * 2.0 / (cx + cy);

    glUniform1f(m_uX, ratioX);
    glUniform1f(m_uY, ratioY);
    glUniform1f(m_uLen, len);
    
    float ox = len * std::cos(ang1) * std::cos(ang2) + cenx;
    float oy = len * std::sin(ang2) + ceny;
    float oz = len * std::sin(ang1) * std::cos(ang2) + cenz;
    glUniform3f(m_uOrigin, ox, oy, oz);

    glUniform3f(m_uRight, std::sin(ang1), 0.0f, -std::cos(ang1));
    glUniform3f(m_uUp, -std::sin(ang2) * std::cos(ang1), std::cos(ang2), -std::sin(ang2) * std::sin(ang1));
    glUniform3f(m_uForward, -std::cos(ang1) * std::cos(ang2), -std::sin(ang2), -std::sin(ang1) * std::cos(ang2));

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Blit to screen
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldDrawFBO);
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBlitFramebuffer(0, 0, FBO_SIZE, FBO_SIZE,
                      m_offsetX, m_offsetY, m_offsetX + m_viewportSize, m_offsetY + m_viewportSize,
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);
                      
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldDrawFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, oldReadFBO);
}

