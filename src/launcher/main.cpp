// The one executable of both games. Legend of Grimrock and Legend of Grimrock 2 are
// reconstructed as two engines from the same tree (src/ and the src2/ overlay), each built as
// a module of its own, libgrimrock1.so and libgrimrock2.so next to this file. The launcher
// decides which game to run and where its data is, loads that module and hands it the
// command line; only one module is ever loaded into the process.
//
// Without a window, when the choice is clear:
//   --game 1 | --game 2 (or --game=N) on the command line;
//   the name the executable was started as: grimrock2 (a symlink or a copy) runs the
//   second game;
//   the data of exactly one game next to the executable (a Steam or GOG install).
// Otherwise -- both games next to it, none, or --launcher -- a window lists the games found
// (next to the launcher, in a Steam library, a GOG install or a folder chosen before) and
// lets the player pick one or locate its folder. The chosen game then starts in a fresh
// process: the launcher runs itself again with --game N --data <folder>.
//
// --data <folder> tells the game where its data is (it defaults to where the game was
// found). The launcher's own switches are not passed on to the game.
#include "GameLibrary.h"
#include "LauncherWindow.h"
#include <SDL3/SDL.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <unistd.h>
#include <vector>

using namespace launcher;

namespace
{

constexpr const char* EntryPoint = "grimrock_game_main";
typedef int (*GameMain)(int argc, char** argv);

void fail(const std::string& message)
{
    fprintf(stderr, "%s\n", message.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "LibreGrimrock", message.c_str(), nullptr);
}

std::string executablePath()
{
    char path[4096];
    ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (length <= 0)
        return "";
    path[length] = 0;
    return path;
}

std::string directoryOf(const std::string& path)
{
    size_t slash = path.rfind('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

// a game number, or -1 for anything that is not one
int parseGameNumber(const char* text)
{
    char* end = nullptr;
    long number = strtol(text, &end, 10);
    return end != text && *end == 0 && number > 0 && number < 100 ? (int)number : -1;
}

// the launcher's switches, taken out of the command line
struct Options
{
    int game = 0;        // --game N
    std::string data;    // --data <folder>
    bool window = false; // --launcher
    bool invalid = false;
};

Options takeOptions(std::vector<char*>& args)
{
    Options options;
    for (size_t i = 1; i < args.size();)
    {
        std::string arg = args[i];
        size_t taken = 0;
        if (arg.compare(0, 7, "--game=") == 0)
        {
            options.game = parseGameNumber(arg.c_str() + 7);
            taken = 1;
        }
        else if (arg == "--game" && i + 1 < args.size())
        {
            options.game = parseGameNumber(args[i + 1]);
            taken = 2;
        }
        else if (arg.compare(0, 7, "--data=") == 0)
        {
            options.data = arg.substr(7);
            taken = 1;
        }
        else if (arg == "--data" && i + 1 < args.size())
        {
            options.data = args[i + 1];
            taken = 2;
        }
        else if (arg == "--launcher")
        {
            options.window = true;
            taken = 1;
        }
        if (taken)
            args.erase(args.begin() + i, args.begin() + i + taken);
        else
            ++i;
    }
    if (options.game && !findGame(options.game))
        options.invalid = true; // not a number, or not a game we know
    return options;
}

// the game the command line or the executable's name asks for, or 0
int requestedGame(const Options& options, const char* argv0)
{
    if (options.game)
        return options.game;
    const char* name = strrchr(argv0, '/');
    name = name ? name + 1 : argv0;
    return strstr(name, "grimrock2") ? 2 : 0;
}

// loads the engine of the game and runs it; returns only when it could not start
int runGame(const GameInfo& game, const std::string& dataDirectory,
            const std::string& launcherDirectory, std::vector<char*> args)
{
    setenv("GRIMROCK_DATA_DIR", dataDirectory.c_str(), 1);
    // Steam's API relaunches a game started outside of Steam unless it knows the app;
    // this is what steam_appid.txt next to the executable says
    setenv("SteamAppId", game.steamAppId, 0);
    setenv("SteamGameId", game.steamAppId, 0);
    std::string module = launcherDirectory + "/" + game.module;
    void* handle = dlopen(module.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle)
    {
        fail(std::string("Cannot load the engine of ") + game.title + ": " + dlerror());
        return 1;
    }
    GameMain gameMain = (GameMain)dlsym(handle, EntryPoint);
    if (!gameMain)
    {
        fail(std::string(game.module) + " has no " + EntryPoint);
        return 1;
    }
    args.push_back(nullptr);
    return gameMain((int)args.size() - 1, args.data());
}

// the window; the chosen game starts in a new process, so it does not inherit the window
int runLauncherWindow(GameLibrary& library, const std::vector<char*>& args)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fail(std::string("Cannot open the launcher window: ") + SDL_GetError());
        return 1;
    }
    int game;
    {
        LauncherWindow window(library);
        game = window.run();
    }
    SDL_Quit();
    if (!game)
        return 0;
    std::string number = std::to_string(game);
    std::string data = library.getInstall(game).directory;
    std::vector<char*> command = {args[0], (char*)"--game", (char*)number.c_str(), (char*)"--data",
                                  (char*)data.c_str()};
    command.insert(command.end(), args.begin() + 1, args.end());
    command.push_back(nullptr);
    std::string self = executablePath();
    execv(self.c_str(), command.data());
    fail(std::string("Cannot start the game: ") + strerror(errno));
    return 1;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<char*> args(argv, argv + argc);
    Options options = takeOptions(args);
    if (options.invalid)
    {
        fail("--game takes 1 (Legend of Grimrock) or 2 (Legend of Grimrock 2).");
        return 1;
    }
    std::string launcherDirectory = directoryOf(executablePath());
    GameLibrary library(launcherDirectory);

    if (!options.window)
    {
        int number = requestedGame(options, args[0]);
        if (!number)
        {
            // exactly one game next to the launcher: it is that game's install
            int found = 0, count = 0;
            for (const GameInfo& game : Games)
                if (GameLibrary::hasData(game, launcherDirectory))
                {
                    found = game.number;
                    ++count;
                }
            if (count == 1)
                number = found;
        }
        if (number)
        {
            const GameInfo& game = *findGame(number);
            std::string data =
                !options.data.empty() ? options.data : library.getInstall(number).directory;
            if (!data.empty() && GameLibrary::hasData(game, data))
                return runGame(game, data, launcherDirectory, args);
            if (!options.data.empty())
            {
                fail(std::string("No ") + game.archive + " in " + options.data + ".");
                return 1;
            }
            // not found anywhere: the window lets the player locate it
        }
    }
    return runLauncherWindow(library, args);
}
