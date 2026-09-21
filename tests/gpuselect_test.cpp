// Tests the discrete GPU choice of selectRenderGPU() on synthetic sysfs trees.
#include "GpuSelect.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static int g_failures = 0;
#define CHECK(cond)                                                                                \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                 \
            ++g_failures;                                                                          \
        }                                                                                          \
    } while (0)

static std::string g_root;

static void writeFile(const std::string& path, const char* text)
{
    FILE* f = fopen(path.c_str(), "w");
    if (f)
    {
        fputs(text, f);
        fclose(f);
    }
}

// A cardN whose device link points at pci/<address> with the given attributes.
static void addGpu(const char* layout, const char* card, const char* vendor, bool bootVga,
                   const char* driver, const char* pci, const char* vram)
{
    std::string root = g_root + "/" + layout;
    std::string dev = root + "/pci/" + pci;
    mkdir(root.c_str(), 0755);
    mkdir((root + "/pci").c_str(), 0755);
    mkdir((root + "/drivers").c_str(), 0755);
    mkdir((root + "/drivers/" + driver).c_str(), 0755);
    mkdir(dev.c_str(), 0755);
    mkdir((root + "/" + card).c_str(), 0755);
    writeFile(dev + "/vendor", vendor);
    writeFile(dev + "/boot_vga", bootVga ? "1" : "0");
    if (vram)
        writeFile(dev + "/mem_info_vram_total", vram);
    if (symlink((std::string("../drivers/") + driver).c_str(), (dev + "/driver").c_str()) != 0 ||
        symlink((std::string("../pci/") + pci).c_str(), (root + "/" + card + "/device").c_str()) !=
            0)
        printf("symlink failed for %s/%s\n", layout, card);
}

static const char* env(const char* name)
{
    const char* v = getenv(name);
    return v ? v : "";
}

static void runLayout(const char* layout, const char* choice)
{
    unsetenv("DRI_PRIME");
    unsetenv("__GLX_VENDOR_LIBRARY_NAME");
    unsetenv("__NV_PRIME_RENDER_OFFLOAD");
    unsetenv("__VK_LAYER_NV_optimus");
    if (choice)
        setenv("GRIMROCK_GPU", choice, 1);
    else
        unsetenv("GRIMROCK_GPU");
    setenv("GRIMROCK_DRM_SYSFS", (g_root + "/" + layout).c_str(), 1);
    selectRenderGPU();
}

int main()
{
    char tmpl[] = "/tmp/gpuselect_test_XXXXXX";
    g_root = mkdtemp(tmpl);

    addGpu("apu_nv", "card0", "0x1002", true, "amdgpu", "0000:05:00.0", "536870912");
    addGpu("apu_nv", "card1", "0x10de", false, "nvidia", "0000:01:00.0", 0);
    runLayout("apu_nv", 0);
    CHECK(strcmp(env("__GLX_VENDOR_LIBRARY_NAME"), "nvidia") == 0);
    CHECK(strcmp(env("__NV_PRIME_RENDER_OFFLOAD"), "1") == 0);

    addGpu("intel_amd", "card0", "0x8086", true, "i915", "0000:00:02.0", 0);
    addGpu("intel_amd", "card1", "0x1002", false, "amdgpu", "0000:03:00.0", "8589934592");
    runLayout("intel_amd", 0);
    CHECK(strcmp(env("DRI_PRIME"), "pci-0000_03_00_0") == 0);
    CHECK(env("__GLX_VENDOR_LIBRARY_NAME")[0] == 0);

    // the discrete GPU already drives the display: nothing to do
    addGpu("desktop", "card0", "0x10de", true, "nvidia", "0000:01:00.0", 0);
    addGpu("desktop", "card1", "0x8086", false, "i915", "0000:00:02.0", 0);
    runLayout("desktop", 0);
    CHECK(env("DRI_PRIME")[0] == 0 && env("__GLX_VENDOR_LIBRARY_NAME")[0] == 0);

    addGpu("apu_amd", "card0", "0x1002", true, "amdgpu", "0000:06:00.0", "536870912");
    addGpu("apu_amd", "card1", "0x1002", false, "amdgpu", "0000:01:00.0", "17179869184");
    runLayout("apu_amd", 0);
    CHECK(strcmp(env("DRI_PRIME"), "pci-0000_01_00_0") == 0);

    addGpu("single", "card0", "0x1002", true, "amdgpu", "0000:01:00.0", "8589934592");
    runLayout("single", 0);
    CHECK(env("DRI_PRIME")[0] == 0);

    addGpu("nouveau", "card0", "0x8086", true, "i915", "0000:00:02.0", 0);
    addGpu("nouveau", "card1", "0x10de", false, "nouveau", "0000:01:00.0", 0);
    runLayout("nouveau", 0);
    CHECK(strcmp(env("DRI_PRIME"), "pci-0000_01_00_0") == 0);

    // explicit choices
    runLayout("intel_amd", "integrated");
    CHECK(env("DRI_PRIME")[0] == 0);
    runLayout("apu_nv", "0000:05:00.0");
    CHECK(strcmp(env("DRI_PRIME"), "pci-0000_05_00_0") == 0);
    runLayout("apu_nv", "bogus");
    CHECK(env("DRI_PRIME")[0] == 0 && env("__GLX_VENDOR_LIBRARY_NAME")[0] == 0);

    // a preset environment is respected
    setenv("DRI_PRIME", "1", 1);
    unsetenv("GRIMROCK_GPU");
    setenv("GRIMROCK_DRM_SYSFS", (g_root + "/apu_nv").c_str(), 1);
    selectRenderGPU();
    CHECK(env("__GLX_VENDOR_LIBRARY_NAME")[0] == 0);

    std::string cleanup = "rm -rf " + g_root;
    if (system(cleanup.c_str()) != 0)
        printf("cleanup failed\n");
    printf(g_failures ? "%d failures\n" : "OK (%d failures)\n", g_failures);
    return g_failures ? 1 : 0;
}
