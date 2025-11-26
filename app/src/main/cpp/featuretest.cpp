// Write C++ code here.
//
// Do not forget to dynamically load the C++ library into your application.
//
// For instance,
//
// In MainActivity.java:
//    static {
//       System.loadLibrary("featuretest");
//    }
//
// Or, in MainActivity.kt:
//    companion object {
//      init {
//         System.loadLibrary("featuretest")
//      }
//    }
#include <CL/opencl.hpp>

std::tuple<cl::Platform, cl::Device> test_func() {
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    for(auto& platform: platforms) {
        std::vector<cl::Device> devices;
        platform.getDevices(CL_DEVICE_TYPE_GPU, &devices);
        for(auto& device: devices) {
            return { platform, device };
        }
    }

    return {};
}
