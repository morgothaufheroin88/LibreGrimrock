// Vendor independent choice of the rendering GPU on multi-GPU systems (not part of the
// original: it always used whatever GLX picked).
#pragma once

// Inspects the DRM devices in sysfs and, when a discrete GPU exists next to the one that
// drives the display, asks the driver stack to render on it through the PRIME offload
// environment (DRI_PRIME for Mesa drivers, __NV_PRIME_RENDER_OFFLOAD for the NVIDIA
// driver). GRIMROCK_GPU=integrated keeps the display GPU, GRIMROCK_GPU=<pci address>
// (e.g. 0000:01:00.0) picks a device; a preset DRI_PRIME or __GLX_VENDOR_LIBRARY_NAME is
// left alone. Must run before SDL is initialised.
void selectRenderGPU();
