// Reconstructed from Grimrock.bin.x86 main.cpp (0x0815dff0).
#include "RapidEngine.h"
#include "core/ArchiveFileSystem.h"
#include "core/Exception.h"
#include "core/FileSystem.h"
#include "core/Sys.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>

// ARC4 key of grimrock.dat (0x0815dff0).
static const char g_archiveKey[16] = {
    0x4b, (char)0x8a, 0x11,       0x56,       (char)0xfd, 0x42,       (char)0xce, (char)0xf3,
    0x00, (char)0xd7, (char)0xa2, (char)0xdf, (char)0xef, (char)0xd4, (char)0xcc, (char)0xf7};
static const char* const g_archiveName = "grimrock.dat";
static const char* const g_iconName = "grimrock.png";

// The original used binreloc (br_init / br_find_exe_dir) to change to the executable
// directory before mounting the archive.
static void changeToExeDir()
{
    const char* base = SDL_GetBasePath();
    if (base && chdir(base) != 0)
        fprintf(stderr, "chdir(%s) failed\n", base);
}

// On hybrid (Intel + NVIDIA) laptops GLX picks the integrated GPU unless PRIME render
// offload is requested through the environment. When the NVIDIA GLX vendor library is
// installed and the user did not choose a vendor, ask for the discrete GPU (like the
// Steam client does with its "Run with NVIDIA" launch option). GRIMROCK_GPU=integrated
// keeps the default.
static void selectDiscreteGPU()
{
    const char* choice = getenv("GRIMROCK_GPU");
    if (getenv("__GLX_VENDOR_LIBRARY_NAME") || (choice && strcmp(choice, "integrated") == 0))
        return;
    // found through the loader's search path so it also works inside the Steam Linux
    // Runtime container, where the host driver is mapped to a different directory
    void* nvidiaGLX = dlopen("libGLX_nvidia.so.0", RTLD_LAZY | RTLD_NOLOAD);
    if (!nvidiaGLX)
        nvidiaGLX = dlopen("libGLX_nvidia.so.0", RTLD_LAZY);
    if (!nvidiaGLX)
        return;
    dlclose(nvidiaGLX);
    setenv("__NV_PRIME_RENDER_OFFLOAD", "1", 0);
    setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 0);
    setenv("__VK_LAYER_NV_optimus", "NVIDIA_only", 0);
}

int main(int argc, char** argv)
{
    selectDiscreteGPU();
    changeToExeDir();
    try
    {
        core::ArchiveFileSystem archive(g_archiveName);
        archive.setEncryptionKey(g_archiveKey, sizeof(g_archiveKey));
        core::mount(archive, 0);
        RapidEngine* engine = new RapidEngine;
        engine->setWindowIcon(g_iconName);
        engine->enterMainLoop();
        delete engine;
    }
    catch (core::Exception& e)
    {
        core::sysMessageBox("Software Failure", e.getReason(), core::MessageBox_Ok);
        return 1;
    }
    _Exit(0);
}
