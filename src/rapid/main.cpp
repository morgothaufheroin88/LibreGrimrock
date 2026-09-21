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
#include <unistd.h>

// ARC4 key of the archive (0x0815dff0; grimrock2.exe 0x00401250 uses the same key although
// grimrock2.dat is stored unencrypted).
static const char g_archiveKey[16] = {
    0x4b, (char)0x8a, 0x11,       0x56,       (char)0xfd, 0x42,       (char)0xce, (char)0xf3,
    0x00, (char)0xd7, (char)0xa2, (char)0xdf, (char)0xef, (char)0xd4, (char)0xcc, (char)0xf7};
#if GRIMROCK_GAME == 2
static const char* const g_archiveName = "grimrock2.dat";
static const char* const g_iconName = "grimrock2.png";
#else
static const char* const g_archiveName = "grimrock.dat";
static const char* const g_iconName = "grimrock.png";
#endif

// The original used binreloc (br_init / br_find_exe_dir) to change to the executable
// directory before mounting the archive.
static void changeToExeDir()
{
    const char* base = SDL_GetBasePath();
    if (base && chdir(base) != 0)
        fprintf(stderr, "chdir(%s) failed\n", base);
}

int main(int argc, char** argv)
{
    changeToExeDir();
    try
    {
        core::ArchiveFileSystem archive(g_archiveName);
        archive.setEncryptionKey(g_archiveKey, sizeof(g_archiveKey));
        core::mount(archive, 0);
        RapidEngine* engine = new RapidEngine;
#if GRIMROCK_GAME == 2
        // grimrock2.exe takes its icon from the executable's resources (LoadIcon 0x65);
        // tools/log2/icon.py writes it out as grimrock2.png
        if (core::sysFileExists(g_iconName))
#endif
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
