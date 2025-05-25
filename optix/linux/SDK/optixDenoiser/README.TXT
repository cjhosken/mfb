Single frame denoising example. Denoise all AOVs (-A) for a single image:
optixDenoiser -a data/albedo.exr \
              -A data/diffuse.exr -A data/glossy.exr -A data/specular.exr \
              -o result.exr data/beauty.exr

When adding (compositing) the RGB pixels in the output files
    result_diffuse_denoised.exr, result_glossy_denoised.exr and result_specular_denoised.exr,
the resulting image should look very close to result.exr (the denoised beauty layer).

optixDenoiser -k enables the inference model used for AOVs (so called kernel prediction),
even if no AOVs are given. The beauty layer is always denoised in this model, in addition to the AOVs.
This model preserves colors much better and might remove low frequency noise better compared to the
non-AOV inference model.

---------------------------------------------------------------------------------------------------
Temporal denoising using direct prediction:
optixDenoiser -F 1-20 -a motiondata/soane-BSDF-+++.exr \
                      -n motiondata/soane-Normal-+++.exr \
                      -f motiondata/soane-Flow-+++.exr \
                      -o result-+++.exr motiondata/soane-Beauty-+++.exr

Add -k for selecting the kernel prediction denoising mode instead of direct prediction.

It is recommended to add -e -8.6 to the command line when creating a clip with ffmpeg (exposure
control). A clip can be created with ffmpeg -i result-%03d.exr soane.mp4

To get a clip from the noisy beauty images first multiply the images by pow(2, -8.6) for example
with oiiotool (part of the OpenImageIO library):
oiiotool --frames 1-20 --cmul 2.577163e-3 motiondata/soane-Beauty-%03d.exr -o noisy%03d.exr
ffmpeg -i noisy%03d.exr noisy.mp4

---------------------------------------------------------------------------------------------------
Checking motion vectors:
Add -z to the temporal denoising command line above. With this option flow vectors from frame N
will be applied to the image in frame N-1 to get the frame N image.
Flow vectors are correct when the result image of frame N is almost congruent with the noisy frame N
image (i.e. no shifts).
