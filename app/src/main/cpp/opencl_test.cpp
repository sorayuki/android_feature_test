#include <jni.h>
#include <android/log.h>

#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 200
#include <CL/opencl.hpp>

#include <thread>
#include <vector>
#include <random>
#include <sstream>

struct OpenCLTest {
    cl::Platform platform;
    cl::Device device;
    cl::Context context;

    cl::CommandQueue createQueue() {
        //cl_command_queue_properties props = CL_QUEUE_PROFILING_ENABLE;
        //return { context, device, props };
        return { context, device };
    }
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

struct TestCase {
    OpenCLTest* ptr;
    cl::CommandQueue queue;

    TestCase(OpenCLTest* p): ptr(p) {
        queue = ptr->createQueue();
    }

    virtual ~TestCase() {}
    virtual void Prepare() = 0;
    virtual std::vector<cl::Event> Run(int loopCount) = 0;
};

struct TestCopyClass: TestCase {
    using TestCase::TestCase;

    static constexpr size_t MB = 1048576;
    static constexpr size_t BUFFER_SIZE = 2; // in uint32_t count
    static constexpr size_t VECSIZE = 16;

    cl::Buffer sourceBuffer;
    cl::Buffer dstBuffer;
    cl::Program prg;
    bool useKernel = true;

    static constexpr const char* src = R"__(
        kernel void copy_buffer(global uint16* input, global uint16* output) {
            int gid = get_global_linear_id();
            output[gid] = input[gid];
        }
    )__";

    void Prepare() override {
        try {
            sourceBuffer = cl::Buffer(ptr->context, CL_MEM_READ_ONLY, BUFFER_SIZE * MB * sizeof(cl_uint16));
            auto pBuffer = (uint32_t*)queue.enqueueMapBuffer(sourceBuffer, true, CL_MAP_WRITE_INVALIDATE_REGION, 0, BUFFER_SIZE * MB * sizeof(cl_uint16));
            fill_random(pBuffer, BUFFER_SIZE * MB * VECSIZE);
            queue.enqueueUnmapMemObject(sourceBuffer, pBuffer);
            dstBuffer = cl::Buffer(ptr->context, CL_MEM_WRITE_ONLY, BUFFER_SIZE * MB * sizeof(cl_uint16));

            prg = cl::Program(ptr->context, src, true);
        } catch(const cl::BuildError& e) {
            for(auto& x: e.getBuildLog()) {
                __android_log_write(ANDROID_LOG_ERROR, "SORAYUKI", x.second.c_str());
            }
        }
    }

    std::vector<cl::Event> Run(int loopCount) override {
        std::vector<cl::Event> events;
        events.reserve(loopCount);
        cl::KernelFunctor<cl::Buffer, cl::Buffer> copyBuffer(prg, "copy_buffer");

        for (int i = 0; i < loopCount; ++i) {
            if (useKernel)
                events.push_back(copyBuffer(cl::EnqueueArgs(queue, cl::NDRange(BUFFER_SIZE, MB)), sourceBuffer, dstBuffer));
            else {
                cl::Event ev;
                queue.enqueueCopyBuffer(sourceBuffer, dstBuffer, 0, 0, BUFFER_SIZE * MB * sizeof(cl_uint16), nullptr, &ev);
                events.push_back(ev);
            }
        }
        return events;
    }
};


struct TestFlopsClass: TestCase {
    using TestCase::TestCase;

    // 转换到 half的输入
    cl::Buffer inputData;
    // 转换到 half的卷积核
    cl::Buffer convKernData;
    // 输出缓冲区
    cl::Buffer outputData;
    // kernel程序
    cl::Program prg;

    static constexpr const char* src = R"__(
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
    int x = get_global_id(0); // 0 ~ 990
    int y = get_global_id(1); // 0 ~ 990
    half16 sum = (half16)0.0;
    #pragma unroll 10
    for(int j = 0; j < 10; ++j) {
        #pragma unroll 10
        for(int i = 0; i < 10; ++i) {
            half16 lhs = inbuf[(y + j) * 990 + (x + i)];
            half16 rhs = kern[j * 10 + i];
            sum += matmul(lhs, rhs);
        }
    }
    outbuf[y * 990 + x] = sum;
}
)__";

    void Prepare() {
        try {
            // 创建缓冲区
            // 输入缓冲区，1000x1000个float16
            cl::Buffer rawInput = cl::Buffer(ptr->context, CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY, 1000 * 1000 * sizeof(cl_float16));
            inputData = cl::Buffer(ptr->context, CL_MEM_READ_WRITE, 1000 * 1000 * sizeof(cl_half16));
            // 100x100的卷积核，float16
            cl::Buffer rawConvKern = cl::Buffer(ptr->context, CL_MEM_READ_ONLY, 10 * 10 * sizeof(cl_float16));
            convKernData = cl::Buffer(ptr->context, CL_MEM_READ_WRITE, 10 * 10 * sizeof(cl_half16));
            outputData = cl::Buffer(ptr->context, CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY, 990 * 990 * sizeof(cl_half16));

            // 填充输入缓冲区
            auto inbuffer = (cl_float*)queue.enqueueMapBuffer(rawInput, true, CL_MAP_WRITE_INVALIDATE_REGION, 0, 1000 * 1000 * sizeof(cl_float16));
            fill_random(inbuffer, 1000 * 1000 * 16);
            queue.enqueueUnmapMemObject(rawInput, inbuffer);

            // 填充卷积核
            auto kernbuffer = (cl_float*)queue.enqueueMapBuffer(rawConvKern, true, CL_MAP_WRITE_INVALIDATE_REGION, 0, 10 * 10 * sizeof(cl_float16));
            fill_random(kernbuffer, 10 * 10 * 16);
            queue.enqueueUnmapMemObject(rawConvKern, kernbuffer);

            // 编译kernel程序
            prg = cl::Program(ptr->context, src, true);

            // 输入数据从单精度浮点转换到半精度浮点
            cl::KernelFunctor<cl::Buffer, cl::Buffer> f32_to_f16(prg, "f32_to_f16");
            f32_to_f16(cl::EnqueueArgs(queue, cl::NDRange(1000, 1000, 16)), rawInput, inputData);
            f32_to_f16(cl::EnqueueArgs(queue, cl::NDRange(10, 10, 16)), rawConvKern, convKernData);
            queue.finish();
        } catch(cl::BuildError& e) {
            for(auto& x: e.getBuildLog()) {
                __android_log_write(ANDROID_LOG_ERROR, "SORAYUKI", x.second.c_str());
            }
        }
    }

    std::vector<cl::Event> Run(int loopCount) override {
        std::vector<cl::Event> events;
        events.reserve(loopCount);
        cl::KernelFunctor<cl::Buffer, cl::Buffer, cl::Buffer> do_convolution(prg, "do_convolution");

        // 计算卷积
        for(int it = 0; it < loopCount; ++it) {
            events.push_back(do_convolution(cl::EnqueueArgs(queue, cl::NDRange(990, 990)), inputData, convKernData, outputData));
        }
        return events;
    }
};

// return: cost in milliseconds
template<class T>
double RunTest(OpenCLTest* ptr, int parallelCount, int loopCount) {
    if (parallelCount <= 0)
        parallelCount = 1;

    std::vector<std::optional<T>> testcases(parallelCount);
    for (auto &tc : testcases) {
        tc = {{ ptr }};
        tc->Prepare();
    }

    using clock = std::chrono::high_resolution_clock;
    auto start = clock::now();

    std::vector<cl::Event> allEvents;
    for (int i = 0; i < parallelCount; ++i) {
        auto evs = testcases[i]->Run(loopCount);
        allEvents.insert(allEvents.end(), evs.begin(), evs.end());
    }

    if (allEvents.empty()) {
        __android_log_write(ANDROID_LOG_WARN, "SORAYUKI", "No OpenCL events captured; returning 0 ms");
        return 0.0;
    }

    cl::WaitForEvents(allEvents);
    auto finished = clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(finished - start).count();
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

    if (strcmp(strTestType, "copy") == 0) {
        auto parallelCount = 1;
        int loopCount = 300;
        auto costMs = RunTest<TestCopyClass>(ptr, parallelCount, loopCount);
        if (costMs <= 0.0)
            return 0.0;
        return TestCopyClass::BUFFER_SIZE * sizeof(cl_uint16) * loopCount * 1000.0 * parallelCount / (double)costMs; // 50MB * sizeof(cl_int) * loopCount / seconds
    } 
    else if (strcmp(strTestType, "flops") == 0) {
        auto parallelCount = std::thread::hardware_concurrency();
        int loopCount = 10;
        auto costMs = RunTest<TestFlopsClass>(ptr, parallelCount, loopCount);
        if (costMs <= 0.0)
            return 0.0;
         // 每次卷积的计算量约为 输入数据(990 * 990) * 卷积核(10 * 10) * 每个元素大小(16) * 每次乘和加外加最后加的运算量(4 + 3 + 1) 次浮点运算
         double totalFlops = 1.0 * 990 * 990 * 10 * 10 * 16 * (4 + 3 + 1) * loopCount * parallelCount;
         return totalFlops * 1000.0 / (double)costMs;
    }
    return 0;
}
