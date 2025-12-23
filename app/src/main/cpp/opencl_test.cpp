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

    cl::Program prg;
    cl::Buffer outBuffer;
    
    static constexpr int globalSize = 2048 * 2048; 
    static constexpr int innerLoop = 3000;

    static constexpr const char* src = R"__(
#pragma OPENCL EXTENSION cl_khr_fp16 : enable
kernel void compute_flops(global half* outbuf, int loops) {
    int id = get_global_linear_id();
    
    // Using half16 to maximize register usage and mimic matrix-mul workload
    // Adreno 840 likely has specialized DOT product / Tensor units
    // Initialize with runtime ID to prevent optimization
    half val = (half)(id & 0xFF) * 0.001h;
    
    half16 a = (half16)(val);
    half16 b = (half16)(1.001h);
    half16 c = (half16)(0.5h);
    
    // Accumulators for dot product results
    half4 sum0 = (half4)(0.0h);
    half4 sum1 = (half4)(0.0h);
    half4 sum2 = (half4)(0.0h);
    half4 sum3 = (half4)(0.0h);

    for(int i = 0; i < loops; ++i) {
        // Mimic the math intensity of a 4x4 matrix multiplication or convolution
        // Use standard 'dot' built-in which maps to hardware dot-product/tensor units efficiently
        // Each dot(half4, half4) is considered 8 ops (4 muls + 4 adds convention for peak FLOPs counting)
        
        // We perform dots across various swizzles to keep execution ports busy and simulate dependencies
        // 16 dots per unroll block
        
        sum0.x += dot(a.lo.lo, b.lo.lo) + dot(a.lo.hi, b.lo.hi);
        sum0.y += dot(a.hi.lo, b.hi.lo) + dot(a.hi.hi, b.hi.hi);
        sum0.z += dot(a.lo.lo, b.hi.lo) + dot(a.lo.hi, b.hi.hi);
        sum0.w += dot(a.hi.lo, b.lo.lo) + dot(a.hi.hi, b.lo.hi);
        
        sum1.x += dot(a.lo.lo, c.lo.lo) + dot(a.lo.hi, c.lo.hi);
        sum1.y += dot(a.hi.lo, c.hi.lo) + dot(a.hi.hi, c.hi.hi);
        sum1.z += dot(a.lo.lo, c.hi.lo) + dot(a.lo.hi, c.hi.hi);
        sum1.w += dot(a.hi.lo, c.lo.lo) + dot(a.hi.hi, c.lo.hi);
        
        // Mutate 'a' to prevent loop invariants being lifted (though dot result accumulation helps)
        a += (half16)(0.0001h);
        
        // Another block
        sum2.x += dot(a.even.even, b.odd.odd) + dot(a.odd.even, b.even.odd);
        sum2.y += dot(a.even.odd, b.odd.even) + dot(a.odd.odd, b.even.even);
        sum2.z += dot(a.lo.lo, b.hi.hi) + dot(a.hi.hi, b.lo.lo);
        sum2.w += dot(a.s0123, b.s3210) + dot(a.s4567, b.s7654); // swizzle fun
        
        sum3.x += dot(a.lo.lo, c.lo.lo) + dot(a.lo.hi, c.lo.hi);
        sum3.y += dot(a.hi.lo, c.hi.lo) + dot(a.hi.hi, c.hi.hi);
        sum3.z += dot(a.lo.lo, c.hi.lo) + dot(a.lo.hi, c.hi.hi);
        sum3.w += dot(a.hi.lo, c.lo.lo) + dot(a.hi.hi, c.lo.hi);
        
        // Total: 32 dot products per loop
    }
    
    half4 total = sum0 + sum1 + sum2 + sum3;
    outbuf[id] = total.x + total.y + total.z + total.w;
}
)__";

    void Prepare() override {
        try {
            // 只分配一个输出buffer防止被优化掉
            outBuffer = cl::Buffer(ptr->context, CL_MEM_WRITE_ONLY, globalSize * sizeof(cl_half)); 
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
        cl::KernelFunctor<cl::Buffer, int> compute_flops(prg, "compute_flops");

        for(int it = 0; it < loopCount; ++it) {
            events.push_back(compute_flops(cl::EnqueueArgs(queue, cl::NDRange(globalSize)), outBuffer, innerLoop));
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
        auto parallelCount = 1;
        int loopCount = 10;
        
        // 256 ops per inner loop (32 dots * 8 ops/dot)
        double opsPerKernel = 256.0 * TestFlopsClass::innerLoop * TestFlopsClass::globalSize;
        
        auto costMs = RunTest<TestFlopsClass>(ptr, parallelCount, loopCount);
        if (costMs <= 0.0)
            return 0.0;
            
         // Total FLOPs = ops_per_kernel * loop_count * parallel_count
         double totalFlops = opsPerKernel * loopCount * parallelCount;
         return totalFlops * 1000.0 / (double)costMs;
    }
    return 0;
}
