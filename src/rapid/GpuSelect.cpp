// Vendor independent GPU selection, see GpuSelect.h.
#include "GpuSelect.h"
#include "core/Array.h"
#include "core/String.h"
#include "core/Sys.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <unistd.h>

using namespace core;

namespace
{

struct GpuInfo
{
    String pciAddress;       // 0000:01:00.0
    String driver;           // i915, xe, amdgpu, radeon, nouveau, nvidia ...
    unsigned vendor;         // PCI vendor id
    bool bootVga;            // drives the console/display: the integrated GPU on laptops
    unsigned long long vram; // dedicated memory when the driver reports it, else 0
};

constexpr unsigned VendorIntel = 0x8086;
constexpr unsigned VendorAMD = 0x1002;
constexpr unsigned VendorNVIDIA = 0x10de;

bool readSysfs(const String& path, char* buffer, size_t size)
{
    FILE* file = fopen(path.c_str(), "r");
    if (!file)
        return false;
    size_t n = fread(buffer, 1, size - 1, file);
    fclose(file);
    buffer[n] = 0;
    while (n > 0 && (buffer[n - 1] == '\n' || buffer[n - 1] == ' '))
        buffer[--n] = 0;
    return true;
}

String linkBasename(const String& path)
{
    char target[512];
    ssize_t n = readlink(path.c_str(), target, sizeof(target) - 1);
    if (n <= 0)
        return String("");
    target[n] = 0;
    const char* slash = strrchr(target, '/');
    return String(slash ? slash + 1 : target);
}

// Every /sys/class/drm/cardN with a PCI device behind it.
void enumerateGpus(Array<GpuInfo>& gpus)
{
    // GRIMROCK_DRM_SYSFS points the scan at a fake tree (tests)
    const char* root = getenv("GRIMROCK_DRM_SYSFS");
    String drm = root ? String(root) : String("/sys/class/drm");
    DIR* dir = opendir(drm.c_str());
    if (!dir)
        return;
    while (dirent* entry = readdir(dir))
    {
        const char* name = entry->d_name;
        if (strncmp(name, "card", 4) != 0 || strchr(name, '-'))
            continue;
        String base = drm + "/" + name + "/device";
        char text[64];
        if (!readSysfs(base + "/vendor", text, sizeof(text)))
            continue;
        GpuInfo gpu;
        gpu.vendor = (unsigned)strtoul(text, 0, 16);
        gpu.pciAddress = linkBasename(base);
        gpu.driver = linkBasename(base + "/driver");
        gpu.bootVga = readSysfs(base + "/boot_vga", text, sizeof(text)) && text[0] == '1';
        gpu.vram = readSysfs(base + "/mem_info_vram_total", text, sizeof(text))
                       ? strtoull(text, 0, 10)
                       : 0;
        bool known = false;
        for (int i = 0; i < gpus.size(); ++i)
            if (gpus[i].pciAddress == gpu.pciAddress)
                known = true;
        if (!known)
            gpus.push_back(gpu);
    }
    closedir(dir);
}

const char* vendorName(unsigned vendor)
{
    switch (vendor)
    {
    case VendorIntel:
        return "Intel";
    case VendorAMD:
        return "AMD";
    case VendorNVIDIA:
        return "NVIDIA";
    default:
        return "unknown vendor";
    }
}

void requestOffload(const GpuInfo& gpu)
{
    if (gpu.driver == "nvidia")
    {
        // proprietary driver: GLX vendor selection plus PRIME render offload
        setenv("__NV_PRIME_RENDER_OFFLOAD", "1", 0);
        setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 0);
        setenv("__VK_LAYER_NV_optimus", "NVIDIA_only", 0);
    }
    else
    {
        // Mesa (amdgpu, radeon, nouveau, i915/xe as the second GPU): DRI_PRIME=pci-<address>
        String id = String("pci-") + gpu.pciAddress;
        id.replace(":", "_");
        id.replace(".", "_");
        setenv("DRI_PRIME", id.c_str(), 0);
    }
    debugPrint("Rendering on GPU %s (%s, %s)\n", gpu.pciAddress.c_str(), vendorName(gpu.vendor),
               gpu.driver.c_str());
}

} // namespace

void selectRenderGPU()
{
    if (getenv("DRI_PRIME") || getenv("__GLX_VENDOR_LIBRARY_NAME") ||
        getenv("__NV_PRIME_RENDER_OFFLOAD"))
        return;
    const char* choice = getenv("GRIMROCK_GPU");
    if (choice && strcmp(choice, "integrated") == 0)
        return;
    Array<GpuInfo> gpus;
    enumerateGpus(gpus);
    if (choice && strcmp(choice, "auto") != 0 && strcmp(choice, "discrete") != 0)
    {
        for (int i = 0; i < gpus.size(); ++i)
            if (gpus[i].pciAddress == choice)
            {
                requestOffload(gpus[i]);
                return;
            }
        debugPrint("GRIMROCK_GPU=%s: no such GPU\n", choice);
        return;
    }
    if (gpus.size() < 2)
        return;
    // The display GPU is the boot VGA device (the first one when nothing is marked). Offload
    // only when the other GPU is clearly the discrete one: the display GPU is an Intel
    // integrated GPU, the other GPU reports more dedicated memory, or an NVIDIA GPU sits
    // next to an AMD display GPU (APU + NVIDIA laptops). A desktop whose discrete GPU
    // already drives the display keeps it.
    int display = 0;
    for (int i = 0; i < gpus.size(); ++i)
        if (gpus[i].bootVga)
            display = i;
    const GpuInfo& shown = gpus[display];
    int discrete = -1;
    for (int i = 0; i < gpus.size(); ++i)
    {
        if (i == display)
            continue;
        const GpuInfo& other = gpus[i];
        bool better = shown.vendor == VendorIntel ||
                      (other.vram > 0 && shown.vram > 0 && other.vram > shown.vram) ||
                      (other.vendor == VendorNVIDIA && shown.vendor == VendorAMD);
        if (better && (discrete < 0 || other.vram > gpus[discrete].vram))
            discrete = i;
    }
    if (discrete >= 0)
        requestOffload(gpus[discrete]);
}
