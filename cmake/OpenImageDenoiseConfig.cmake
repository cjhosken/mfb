
# Manually set OpenEXR paths (adjust as needed)
set(OpenImageDenoise_ROOT="${CMAKE_CURRENT_LIST_DIR}/../deps/openimagedenoise")
set(OpenImageDenoise_INCLUDE_DIR "${OpenImageDenoise_ROOT}/include")
set(OpenImageDenoise_LIB_DIR "${OpenImageDenoise_ROOT}/lib")

# Find OpenEXR libraries (adjust names if needed)
find_library(OpenImageDenoise_LIB OpenImageDenoise PATHS ${OpenImageDenoise_LIB_DIR} NO_DEFAULT_PATH)
find_library(OpenImageDenoise_CORE_LIB OpenImageDenoise_core PATHS ${OpenImageDenoise_LIB_DIR} NO_DEFAULT_PATH)
find_library(OpenImageDenoise_DEVICE_CPU_LIB OpenImageDenoise_device_cpu PATHS ${OpenImageDenoise_LIB_DIR} NO_DEFAULT_PATH)
find_library(OpenImageDenoise_DEVICE_CUDA_LIB OpenImageDenoise_device_cuda PATHS ${OpenImageDenoise_LIB_DIR} NO_DEFAULT_PATH)
find_library(OpenImageDenoise_DEVICE_HIP_LIB OpenImageDenoise_device_hip PATHS ${OpenImageDenoise_LIB_DIR} NO_DEFAULT_PATH)
find_library(OpenImageDenoise_DEVICE_SYCL_LIB OpenImageDenoise_device_sycl PATHS ${OpenImageDenoise_LIB_DIR} NO_DEFAULT_PATH)


# Include OpenEXR headers
include_directories(${OpenImageDenoise_INCLUDE_DIR})

# Link OpenEXR to your target
link_libraries(
    ${OpenImageDenoise_LIB}
    ${OpenImageDenoise_CORE_LIB}
    ${OpenImageDenoise_CPU_LIB}
    ${OpenImageDenoise_CUDA_LIB}
    ${OpenImageDenoise_HIP_LIB}
    ${OpenImageDenoise_SYCL_LIB}
)