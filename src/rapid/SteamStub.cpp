// Replacement for Steam.cpp when the Steamworks SDK is not available at build time: the
// game sees the same module but Steam.init fails like it does without a Steam client.
#include "Steam.h"

static int Steam_init(lua_State* L)
{
    luaL_checkinteger(L, 1);
    lua_pushnil(L);
    lua_pushstring(L, "SteamAPI_Init() failed");
    return 2;
}
static int Steam_update(lua_State* L)
{
    return 0;
}
static int Steam_pollEvents(lua_State* L)
{
    lua_pushnil(L);
    return 1;
}
static int Steam_notAvailable(lua_State* L)
{
    return luaL_error(L, "Steam is not available");
}

void shutdownSteam() {}

void steam_mod(lua_State* L)
{
    static const char* const names[] = {"setOverlayNotificationPosition",
                                        "requestCurrentStats",
                                        "setAchievement",
                                        "getAchievement",
                                        "getAchievementDisplayAttribute",
                                        "storeStats",
                                        "resetAllStats",
                                        "fileWrite",
                                        "fileRead",
                                        "fileForget",
                                        "fileDelete",
                                        "fileExists",
                                        "filePersisted",
                                        "getFileSize",
                                        "getFileCount",
                                        "getFileNameAndSize",
                                        "getQuota",
                                        "isCloudEnabledForAccount",
                                        "isCloudEnabledForApp",
                                        "setCloudEnabledForApp",
                                        "getAPICallStatus",
                                        "getAPICallFailureReason",
                                        "getAPICallResult",
                                        "publishWorkshopFile",
                                        "createPublishedFileUpdateRequest",
                                        "updatePublishedFileFile",
                                        "updatePublishedFilePreviewFile",
                                        "updatePublishedFileTitle",
                                        "updatePublishedFileDescription",
                                        "updatePublishedFileVisibility",
                                        "updatePublishedFileTags",
                                        "commitPublishedFileUpdate",
                                        "deletePublishedFile",
                                        "getPublishedFileDetails",
                                        "subscribePublishedFile",
                                        "unsubscribePublishedFile",
                                        "enumerateUserPublishedFiles",
                                        "enumerateUserSubscribedFiles",
                                        "UGCDownload",
                                        "UGCDownloadProgress",
                                        "UGCRead",
                                        "getFriendPersonaName",
                                        "activateGameOverlayToWebPage",
                                        0};
    luaL_Reg functions[48];
    int n = 0;
    functions[n++] = {"init", Steam_init};
    functions[n++] = {"update", Steam_update};
    functions[n++] = {"pollEvents", Steam_pollEvents};
    for (int i = 0; names[i]; ++i)
        functions[n++] = {names[i], Steam_notAvailable};
    functions[n] = {0, 0};
    luax::registerModule(L, "Steam", functions);
}
