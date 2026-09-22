// The one executable of both games. Legend of Grimrock and Legend of Grimrock 2 are
// reconstructed as two engines from the same tree (src/ and the src2/ overlay), and each is
// built as a module of its own, libgrimrock1.so and libgrimrock2.so next to this file. The
// launcher decides which game to run, loads that module and hands it the command line;
// only one module is ever loaded into the process.
//
// The game is, in this order:
//   --game 1 | --game 2 (or --game=N) on the command line, which is not passed on;
//   the name the executable was started as: grimrock2 (a symlink or a copy) runs the
//   second game;
//   the data next to the executable, grimrock.dat or grimrock2.dat. With both there,
//   the first game runs.
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <unistd.h>
#include <vector>

namespace
{

constexpr const char* EntryPoint = "grimrock_game_main";
typedef int (*GameMain)(int argc, char** argv);

struct Game
{
    int number;
    const char* title;
    const char* archive;
    const char* module;
};
constexpr Game Games[] = {
    {1, "Legend of Grimrock", "grimrock.dat", "libgrimrock1.so"},
    {2, "Legend of Grimrock 2", "grimrock2.dat", "libgrimrock2.so"},
};

void fail(const std::string& message)
{
    fprintf(stderr, "%s\n", message.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Legend of Grimrock", message.c_str(), 0);
}

std::string executableDirectory()
{
    char path[4096];
    ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (length <= 0)
        return ".";
    path[length] = 0;
    char* slash = strrchr(path, '/');
    if (slash)
        *slash = 0;
    return path;
}

bool fileExists(const std::string& path)
{
    return access(path.c_str(), R_OK) == 0;
}

const Game* gameByNumber(int number)
{
    for (const Game& game : Games)
        if (game.number == number)
            return &game;
    return 0;
}

// --game N / --game=N, taken out of the arguments; 0 when absent
int takeGameSwitch(std::vector<char*>& args)
{
    for (size_t i = 1; i < args.size(); ++i)
    {
        const char* arg = args[i];
        if (strncmp(arg, "--game=", 7) == 0)
        {
            int number = atoi(arg + 7);
            args.erase(args.begin() + i);
            return number;
        }
        if (strcmp(arg, "--game") == 0 && i + 1 < args.size())
        {
            int number = atoi(args[i + 1]);
            args.erase(args.begin() + i, args.begin() + i + 2);
            return number;
        }
    }
    return 0;
}

const Game* chooseGame(std::vector<char*>& args, const std::string& directory)
{
    if (int number = takeGameSwitch(args))
    {
        const Game* game = gameByNumber(number);
        if (!game)
            fail("--game takes 1 (Legend of Grimrock) or 2 (Legend of Grimrock 2), not " +
                 std::to_string(number) + ".");
        return game;
    }
    const char* name = strrchr(args[0], '/');
    name = name ? name + 1 : args[0];
    if (strstr(name, "grimrock2"))
        return gameByNumber(2);
    const Game* found = 0;
    for (const Game& game : Games)
        if (!found && fileExists(directory + "/" + game.archive))
            found = &game;
    return found;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<char*> args(argv, argv + argc);
    std::string directory = executableDirectory();
    bool explicitChoice = false;
    for (int i = 1; i < argc; ++i)
        explicitChoice |= strncmp(argv[i], "--game", 6) == 0;
    const Game* game = chooseGame(args, directory);
    if (!game)
    {
        if (!explicitChoice)
            fail("No game to run: put grimrock.dat (Legend of Grimrock) or grimrock2.dat "
                 "(Legend of Grimrock 2) next to " +
                 directory + ", or choose one with --game 1 or --game 2.");
        return 1;
    }
    std::string module = directory + "/" + game->module;
    void* handle = dlopen(module.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle)
    {
        fail(std::string("Cannot load the engine of ") + game->title + ": " + dlerror());
        return 1;
    }
    GameMain gameMain = (GameMain)dlsym(handle, EntryPoint);
    if (!gameMain)
    {
        fail(std::string(game->module) + " has no " + EntryPoint);
        return 1;
    }
    args.push_back(0);
    return gameMain((int)args.size() - 1, args.data());
}
