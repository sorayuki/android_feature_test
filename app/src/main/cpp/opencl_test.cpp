#include <jni.h>
#include <android/log.h>

#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 200
#include <CL/opencl.hpp>

#include <thread>
#include <chrono>
#include <vector>
#include <random>
#include <sstream>

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
                auto platformName = platform.getInfo<CL_PLATFORM_NAME>();
                {
                    std::stringstream tmpbuf;
                    tmpbuf << "OpenCL Platform: " << platformName;
                    __android_log_write(ANDROID_LOG_INFO, "SORAYUKI", tmpbuf.str().c_str());
                }
                std::vector<cl::Device> devices;
                platform.getDevices(CL_DEVICE_TYPE_GPU, &devices);
                for (auto &device: devices) {
                    if (!ptr->device())
                        ptr->device = device;
                    if (!ptr->platform())
                        ptr->platform = platform;
                    {
                        auto deviceName = device.getInfo<CL_DEVICE_NAME>();
                        std::stringstream tmpbuf;
                        tmpbuf << "\tDevice: " << deviceName;
                        __android_log_write(ANDROID_LOG_INFO, "SORAYUKI", tmpbuf.str().c_str());
                    }
                }
            }
            if (ptr->platform() && ptr->device())
                return true;
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
    jboolean is_key_copy = JNI_FALSE;
    auto pKey = env->GetStringUTFChars(key, &is_key_copy);
    std::shared_ptr<int> guard{(int*)1024, [=](int*){
        if (is_key_copy == JNI_TRUE)
            env->ReleaseStringUTFChars(key, pKey);
    }};

    try {
        if (strcmp(pKey, "device_name") == 0) {
            auto name = ptr->device.getInfo<CL_DEVICE_NAME>();
            return env->NewStringUTF(name.c_str());
        } else if (strcmp(pKey, "platform_name") == 0) {
            auto name = ptr->platform.getInfo<CL_PLATFORM_NAME>();
            return env->NewStringUTF(name.c_str());
        } else if (strcmp(pKey, "device_exts") == 0) {
            auto extensions = ptr->device.getInfo<CL_DEVICE_EXTENSIONS>();
            return env->NewStringUTF(extensions.c_str());
        } else if (strcmp(pKey, "platform_exts") == 0) {
            auto extensions = ptr->platform.getInfo<CL_PLATFORM_EXTENSIONS>();
            return env->NewStringUTF(extensions.c_str());
        }
    } catch(const cl::Error& e) {
        std::string msg = "[" + std::to_string(e.err()) + "]" + e.what();
        __android_log_write(ANDROID_LOG_ERROR, "SORAYUKI", msg.c_str());
    }

    return env->NewStringUTF("(null)");
}

static void fill_random(cl_uint* ptr, int count) {
    std::random_device randdev;
    std::mt19937 rand(randdev());
    for(int i = 0; i < count; ++i)
        ptr[i] = rand();
}

static void fill_random(cl_float* ptr, int count) {
    std::random_device randdev;
    std::mt19937 rand(randdev());
    for(int i = 0; i < count; ++i)
        ptr[i] = rand() / (float)RAND_MAX;
}

extern "C" JNIEXPORT jfloat JNICALL
Java_net_sorayuki_featuretest_OpenCLTest_TestCopy
(JNIEnv *env, jobject thiz, jlong self, jboolean use_kernel) {
    auto ptr = (OpenCLTest*)self;
    cl::Buffer sourceBuffer(ptr->context, CL_MEM_READ_ONLY, 104857600 * sizeof(uint32_t));
    auto pBuffer = (uint32_t*)ptr->queue.enqueueMapBuffer(sourceBuffer, true, CL_MAP_WRITE, 0, 104857600 * sizeof(uint32_t));
    fill_random(pBuffer, 104857600);
    ptr->queue.enqueueUnmapMemObject(sourceBuffer, pBuffer);
    cl::Buffer dstBuffer(ptr->context, CL_MEM_WRITE_ONLY, 104857600 * sizeof(uint32_t));

    try {
        auto source = R"__(
            kernel void copy_buffer(global int* input, global int* output) {
                int gid = get_global_linear_id();
                output[gid] = input[gid];
            }
        )__";
        auto prg = cl::Program(ptr->context, source, true);
        cl::KernelFunctor<cl::Buffer, cl::Buffer> copyBuffer(prg, "copy_buffer");

        using namespace std::chrono;
        auto start = steady_clock::now();
        for (int i = 0; i < 50; ++i) {
            if (use_kernel)
                copyBuffer(cl::EnqueueArgs(ptr->queue, cl::NDRange(1048576, 100)), sourceBuffer, dstBuffer);
            else
                ptr->queue.enqueueCopyBuffer(sourceBuffer, dstBuffer, 0, 0, 104857600 * sizeof(uint32_t));
        }
        ptr->queue.finish();
        auto costMs = (jlong) duration_cast<milliseconds>(steady_clock::now() - start).count();
        return 100 * sizeof(cl_int) * 50 * 1000.0f / costMs; // 100MB * sizeof(cl_int) * 100 times / seconds
    } catch(const cl::BuildError& e) {
        for(auto& x: e.getBuildLog()) {
            __android_log_write(ANDROID_LOG_ERROR, "SORAYUKI", x.second.c_str());
        }
        return 0;
    }
}

static double TestFlops(OpenCLTest* ptr) {
    // 输入缓冲区，1000x1000个float16
    cl::Buffer rawInput(ptr->context, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, 1000 * 1000 * sizeof(cl_float16));
    // 转换到 half的输入
    cl::Buffer inputData(ptr->context, CL_MEM_READ_WRITE, 1000 * 1000 * sizeof(cl_half16));
    // 100x100的卷积核，float16
    cl::Buffer rawConvKern(ptr->context, CL_MEM_READ_ONLY, 100 * 100 * sizeof(cl_float16));
    // 转换到 half的卷积核
    cl::Buffer convKernData(ptr->context, CL_MEM_READ_WRITE, 100 * 100 * sizeof(cl_half16));
    // 输出缓冲区
    cl::Buffer outputData(ptr->context, CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY, 900 * 900 * sizeof(cl_half16));
    // 填充输入缓冲区
    auto inbuffer = (cl_float*)ptr->queue.enqueueMapBuffer(rawInput, true, CL_MAP_WRITE_INVALIDATE_REGION, 0, 1000 * 1000 * sizeof(cl_float16));
    fill_random(inbuffer, 1000 * 1000 * 16);
    ptr->queue.enqueueUnmapMemObject(rawInput, inbuffer);
    // 填充卷积核
    auto kernbuffer = (cl_float*)ptr->queue.enqueueMapBuffer(rawConvKern, true, CL_MAP_WRITE_INVALIDATE_REGION, 0, 100 * 100 * sizeof(cl_float16));
    fill_random(kernbuffer, 100 * 100 * 16);
    ptr->queue.enqueueUnmapMemObject(rawConvKern, kernbuffer);
    // kernel程序
    try {
        auto src = R"__(
#pragma OPENCL EXTENSION cl_khr_fp16 : enable
kernel void f32_to_f16(global float* inbuf, global half* outbuf) {
    int id = get_global_linear_id();
    outbuf[id] = (half)inbuf[id];
}

half16 matmul(half16 lhs, half16 rhs) {
    half16 res;

    res.lo.lo.odd.odd = dot(lhs.lo.lo, rhs.odd.odd);
    res.lo.hi.odd.odd = dot(lhs.lo.hi, rhs.odd.odd);
    res.hi.lo.odd.odd = dot(lhs.hi.lo, rhs.odd.odd);
    res.hi.hi.odd.odd = dot(lhs.hi.hi, rhs.odd.odd);
    res.lo.lo.even.odd = dot(lhs.lo.lo, rhs.even.odd);
    res.lo.hi.even.odd = dot(lhs.lo.hi, rhs.even.odd);
    res.hi.lo.even.odd = dot(lhs.hi.lo, rhs.even.odd);
    res.hi.hi.even.odd = dot(lhs.hi.hi, rhs.even.odd);
    res.lo.lo.odd.even = dot(lhs.lo.lo, rhs.odd.even);
    res.lo.hi.odd.even = dot(lhs.lo.hi, rhs.odd.even);
    res.hi.lo.odd.even = dot(lhs.hi.lo, rhs.odd.even);
    res.hi.hi.odd.even = dot(lhs.hi.hi, rhs.odd.even);
    res.lo.lo.even.even = dot(lhs.lo.lo, rhs.even.even);
    res.lo.hi.even.even = dot(lhs.lo.hi, rhs.even.even);
    res.hi.lo.even.even = dot(lhs.hi.lo, rhs.even.even);
    res.hi.hi.even.even = dot(lhs.hi.hi, rhs.even.even);

    return res;
}

kernel void do_convolution(global half16* inbuf, global half16* kern, global half16* outbuf) {
    int x = get_global_id(0); // 0 ~ 900
    int y = get_global_id(1); // 0 ~ 900
    half16 sum = (half)0.0;
    for(int j = 0; j < 100; ++j) {
        for(int i = 0; i < 100; ++i) {
            half16 lhs = inbuf[(y + j) * 900 + (x + i)];
            half16 rhs = kern[j * 100 + i];
            sum += matmul(lhs, rhs);
        }
    }
    outbuf[y * 900 + x] = sum;
}
)__";
        cl::Program prg(ptr->context, src, true);
        // 输入数据从单精度浮点转换到半精度浮点
        cl::KernelFunctor<cl::Buffer, cl::Buffer> f32_to_f16(prg, "f32_to_f16");
        f32_to_f16(cl::EnqueueArgs(ptr->queue, cl::NDRange(1000, 1000, 16)), rawInput, inputData);
        f32_to_f16(cl::EnqueueArgs(ptr->queue, cl::NDRange(100, 100, 16)), rawConvKern, convKernData);
        ptr->queue.finish();

        cl::KernelFunctor<cl::Buffer, cl::Buffer, cl::Buffer> do_convolution(prg, "do_convolution");
        // 开始计时
        using namespace std::chrono;
        auto start = steady_clock::now();
        // 计算卷积
        int loopCount = 2;
        for(int it = 0; it < loopCount; ++it) {
            do_convolution(cl::EnqueueArgs(ptr->queue, cl::NDRange(900, 900)), inputData, convKernData, outputData);
        }
        // 计算耗时
        ptr->queue.finish();
        auto costMs = (jlong) duration_cast<milliseconds>(steady_clock::now() - start).count();
        // 计算算力，乘法和加法次数统计
        // 重复执行次数 * kernel执行次数 * 卷积核大小 * 计算次数
        auto mul_count = 1.0 * loopCount * 900 * 900 * 100 * 100 * 16 * 4;
        auto add_count = 1.0 * loopCount * 900 * 900 * 100 * 100 * (16 * 3 + 16);
        return (mul_count + add_count) * 1000.0 / costMs;
    } catch(cl::BuildError& e) {
        for(auto& x: e.getBuildLog()) {
            __android_log_write(ANDROID_LOG_ERROR, "SORAYUKI", x.second.c_str());
        }
        return 0;
    }
}

extern "C" JNIEXPORT jdouble JNICALL
Java_net_sorayuki_featuretest_OpenCLTest_TestCompute
(JNIEnv *env, jobject thiz, jlong self, jstring type) {
    auto ptr = (OpenCLTest*)self;
    jboolean isCopy = JNI_FALSE;
    auto strTestType = env->GetStringUTFChars(type, &isCopy);
    std::shared_ptr<int> guard{(int*)1024, [=](int*){
        if (isCopy == JNI_TRUE)
            env->ReleaseStringUTFChars(type, strTestType);
    }};

    if (strcmp(strTestType, "flops") == 0)
        return TestFlops(ptr);
    return 0;
}