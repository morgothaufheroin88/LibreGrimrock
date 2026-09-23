// Tests of the launcher's game library: where installs are found, in which order, and
// that a folder the player chose is remembered. Everything runs in a temporary HOME.
#include "../src/launcher/GameLibrary.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using namespace launcher;

static int failures = 0;
#define REQUIRE(c)                                                                                 \
    do                                                                                             \
    {                                                                                              \
        if (!(c))                                                                                  \
        {                                                                                          \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                    \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

static void makeDirectories(const std::string& path)
{
    for (size_t slash = path.find('/', 1); slash != std::string::npos;
         slash = path.find('/', slash + 1))
        mkdir(path.substr(0, slash).c_str(), 0755);
    mkdir(path.c_str(), 0755);
}

static void touch(const std::string& path)
{
    std::ofstream(path) << "x";
}

int main()
{
    char root[] = "/tmp/grimrock_launcher_XXXXXX";
    if (!mkdtemp(root))
        return 1;
    std::string home = std::string(root) + "/home";
    std::string launcherDir = std::string(root) + "/launcher";
    makeDirectories(home);
    makeDirectories(launcherDir);
    setenv("HOME", home.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (home + "/.config").c_str(), 1);

    // nothing installed
    {
        GameLibrary library(launcherDir);
        REQUIRE(library.getInstall(1).directory.empty());
        REQUIRE(library.getInstall(2).directory.empty());
    }

    // the second game in a Steam library that libraryfolders.vdf lists
    std::string steam = home + "/.local/share/Steam";
    std::string otherLibrary = std::string(root) + "/games/SteamLibrary";
    makeDirectories(steam + "/steamapps");
    std::ofstream(steam + "/steamapps/libraryfolders.vdf")
        << "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"" << steam
        << "\"\n\t}\n\t\"1\"\n\t{\n\t\t\"path\"\t\t\"" << otherLibrary << "\"\n\t}\n}\n";
    std::string steamGame2 = otherLibrary + "/steamapps/common/Legend of Grimrock 2";
    makeDirectories(steamGame2);
    touch(steamGame2 + "/grimrock2.dat");
    {
        GameLibrary library(launcherDir);
        REQUIRE(library.getInstall(2).directory == steamGame2);
        REQUIRE(library.getInstall(2).source == "Steam");
        REQUIRE(library.getInstall(1).directory.empty());
    }

    // the first game next to the launcher wins over nothing, and a chosen folder over Steam
    touch(launcherDir + "/grimrock.dat");
    std::string chosen = std::string(root) + "/my games/lg2";
    makeDirectories(chosen);
    touch(chosen + "/grimrock2.dat");
    {
        GameLibrary library(launcherDir);
        REQUIRE(library.getInstall(1).directory == launcherDir);
        REQUIRE(library.getInstall(1).source == "next to the launcher");
        REQUIRE(!library.setInstall(2, std::string(root) + "/games")); // no data there
        REQUIRE(library.setInstall(2, chosen));
        REQUIRE(library.getInstall(2).directory == chosen);
    }
    // remembered for the next start
    {
        GameLibrary library(launcherDir);
        REQUIRE(library.getInstall(2).directory == chosen);
        REQUIRE(library.getInstall(2).source == "chosen folder");
    }
    // a remembered folder that lost its data falls back to the other places
    remove((chosen + "/grimrock2.dat").c_str());
    {
        GameLibrary library(launcherDir);
        REQUIRE(library.getInstall(2).directory == steamGame2);
    }

    std::string cleanup = std::string("rm -rf '") + root + "'";
    if (system(cleanup.c_str()) != 0)
        printf("could not remove %s\n", root);
    printf("%s (%d failures)\n", failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
