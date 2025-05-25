This sample demonstrates calculation of optical flow between two images.
Examples:
    optixOpticalFlow -o flow002.exr \
         ../optixDenoiser/motiondata/soane-BSDF-001.exr \
         ../optixDenoiser/motiondata/soane-BSDF-002.exr

    optixOpticalFlow -F 1-20 -o flow+++.exr ../optixDenoiser/motiondata/soane-BSDF-+++.exr

The flow vectors can be used for the OptiX denoiser in temporal mode.
The sample runs only on Ampere and newer GPUs.
