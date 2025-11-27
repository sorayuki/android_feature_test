#include <jni.h>
#include "dlfcn.h"
#include <android/log.h>

#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 200
#include <CL/opencl.hpp>

#include <thread>
#include <chrono>
#include <vector>
#include <random>

struct OpenCLTest {
    cl::Platform platform;
    cl::Device device;
    cl::Context context;
    cl::CommandQueue queue;
};

extern "C" JNIEXPORT jlong JNICALL
Java_net_sorayuki_featuretest_OpenCLTest_Create
(JNIEnv *env, jobject thiz) {
    return (intptr_t)new OpenCLTest{};
}

extern "C" JNIEXPORT void JNICALL
Java_net_sorayuki_featuretest_OpenCLTest_Delete
(JNIEnv *env, jobject thiz, jlong self) {
    delete (OpenCLTest*)self;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_net_sorayuki_featuretest_OpenCLTest_Init
(JNIEnv *env, jobject thiz, jlong self) {
    auto ptr = (OpenCLTest *) self;
    try {
        bool has_device = [&]() {
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
            return false;
        }();

        if (has_device) {
            ptr->context = cl::Context(ptr->device);
            ptr->queue = cl::CommandQueue(ptr->context, ptr->device);
        }

        return has_device;
    }
    catch(const cl::Error& e)
    {
        std::string msg = "[" + std::to_string(e.err()) + "]" + e.what();
        __android_log_write(ANDROID_LOG_ERROR, "SORAYUKI", msg.c_str());
    }

    return false;

}

extern "C" JNIEXPORT jstring JNICALL
Java_net_sorayuki_featuretest_OpenCLTest_QueryString
(JNIEnv *env, jobject thiz, jlong self, jstring key) {
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

extern "C" JNIEXPORT jlong JNICALL
Java_net_sorayuki_featuretest_OpenCLTest_TestHostToDeviceTransfer
(JNIEnv *env, jobject thiz, jlong self, jint times_400MB) {
    auto ptr = (OpenCLTest*)self;
    cl::Buffer hostBuffer(ptr->context, CL_MEM_READ_WRITE, 104857600 * sizeof(uint32_t));
    auto pBuffer = (uint32_t*)ptr->queue.enqueueMapBuffer(hostBuffer, true, CL_MAP_WRITE, 0, 104857600 * sizeof(uint32_t));
    std::random_device randdev;
    std::mt19937 rand(randdev());
    for(int i = 0; i < 104857600; ++i)
        pBuffer[i] = rand();
    ptr->queue.enqueueUnmapMemObject(hostBuffer, pBuffer);

    using namespace std::chrono;
    auto start = steady_clock::now();
    cl::Buffer clbuffer(ptr->context, CL_MEM_READ_WRITE, 104857600 * sizeof(uint32_t));
    for(int i = 0; i < times_400MB; ++i) {
        ptr->queue.enqueueCopyBuffer(hostBuffer, clbuffer, 0, 0, 1048576 * sizeof(uint32_t));
    }
    ptr->queue.finish();
    auto ret = (jlong)duration_cast<milliseconds>(steady_clock::now() - start).count();

    ptr->queue.finish();
    return ret;
}
