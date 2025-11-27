#include <jni.h>
#include "dlfcn.h"
#include <android/log.h>

#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 200
#include <CL/opencl.hpp>

struct OpenCLTest {
    cl::Platform platform;
    cl::Device device;
};

extern "C"
JNIEXPORT jlong JNICALL
Java_net_sorayuki_featuretest_OpenCLTest_nativeCreate(JNIEnv *env, jobject thiz) {
    return (intptr_t)new OpenCLTest{};
}

extern "C"
JNIEXPORT void JNICALL
Java_net_sorayuki_featuretest_OpenCLTest_nativeDelete(JNIEnv *env, jobject thiz, jlong self) {
    delete (OpenCLTest*)self;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_net_sorayuki_featuretest_OpenCLTest_nativeInit(JNIEnv *env, jobject thiz, jlong self) {
    try {
        auto ptr = (OpenCLTest *) self;
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);
        for (auto &platform: platforms) {
            std::vector<cl::Device> devices;
            platform.getDevices(CL_DEVICE_TYPE_GPU, &devices);
            for (auto &device: devices) {
                ptr->device = device;
                ptr->platform = platform;
                return true;
            }
        }
    }
    catch(const cl::Error& e)
    {
        std::string msg = "[" + std::to_string(e.err()) + "]" + e.what();
        __android_log_write(ANDROID_LOG_ERROR, "SORAYUKI", msg.c_str());
    }

    return false;

}

extern "C"
JNIEXPORT jstring JNICALL
Java_net_sorayuki_featuretest_OpenCLTest_queryString(JNIEnv *env, jobject thiz, jlong self, jstring key) {
    auto ptr = (OpenCLTest*)self;
    jboolean is_key_copy = false;
    auto pKey = env->GetStringUTFChars(key, &is_key_copy);
    std::shared_ptr<int> guard{(int*)1024, [=](int*){
        if (is_key_copy)
            env->ReleaseStringUTFChars(key, pKey);
    }};

    try {
        if (strcmp(pKey, "device_name") == 0) {
            auto name = ptr->device.getInfo<CL_DEVICE_NAME>();
            return env->NewStringUTF(name.c_str());
        } else if (strcmp(pKey, "platform_name") == 0) {
            auto name = ptr->platform.getInfo<CL_PLATFORM_NAME>();
            return env->NewStringUTF(name.c_str());
        }
    } catch(const cl::Error& e) {
        std::string msg = "[" + std::to_string(e.err()) + "]" + e.what();
        __android_log_write(ANDROID_LOG_ERROR, "SORAYUKI", msg.c_str());
    }

    return env->NewStringUTF("(null)");
}