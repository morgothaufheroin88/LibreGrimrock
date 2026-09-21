// Reconstructed from Grimrock.bin.x86 Steam.cpp.
#include "Steam.h"
#include "core/Array.h"
#include "core/FileSystem.h"
#include "core/Sys.h"
#include <cstring>
#include <steam/steam_api.h>

using namespace core;

// A callback received from Steam, queued until Lua polls it (0x98 bytes in the original).
struct SteamEvent
{
    enum Type
    {
        UserStatsReceived = 0,
        UserStatsStored = 1,
        AchievementStored = 2,
        FileSubscribed = 3,
        FileUnsubscribed = 4
    };
    int type;
    union
    {
        struct
        {
            EResult result;
        } statsReceived;
        UserStatsStored_t statsStored;
        UserAchievementStored_t achievementStored;
        RemoteStoragePublishedFileSubscribed_t fileSubscribed;
        RemoteStoragePublishedFileUnsubscribed_t fileUnsubscribed;
    };
    SteamEvent()
    {
        memset((void*)this, 0, sizeof(*this));
    }
};

class SteamContext
{
  public:
    SteamContext();
    void update();

    void OnUserStatsReceived(UserStatsReceived_t* pParam);
    void OnUserStatsStored(UserStatsStored_t* pParam);
    void OnAchievementStored(UserAchievementStored_t* pParam);
    void OnPersonaStateChanged(PersonaStateChange_t* pParam);
    void OnFileSubscribed(RemoteStoragePublishedFileSubscribed_t* pParam);
    void OnFileUnsubscribed(RemoteStoragePublishedFileUnsubscribed_t* pParam);

    CCallback<SteamContext, UserStatsReceived_t, false> m_userStatsReceived;
    CCallback<SteamContext, UserStatsStored_t, false> m_userStatsStored;
    CCallback<SteamContext, UserAchievementStored_t, false> m_achievementStored;
    CCallback<SteamContext, PersonaStateChange_t, false> m_personaStateChanged;
    CCallback<SteamContext, RemoteStoragePublishedFileSubscribed_t, false> m_fileSubscribed;
    CCallback<SteamContext, RemoteStoragePublishedFileUnsubscribed_t, false> m_fileUnsubscribed;

    CGameID m_gameId;
    ISteamUserStats* m_pUserStats;
    ISteamRemoteStorage* m_pRemoteStorage;
    Array<SteamEvent> m_events;
};

static SteamContext* g_pSteamContext = 0;

static luax::Enum g_visibilities[] = {
    {"public", k_ERemoteStoragePublishedFileVisibilityPublic},
    {"friendsOnly", k_ERemoteStoragePublishedFileVisibilityFriendsOnly},
    {"private", k_ERemoteStoragePublishedFileVisibilityPrivate},
    {0, 0}};

static luax::Enum g_fileTypes[] = {{"community", k_EWorkshopFileTypeCommunity},
                                   {"microtransaction", k_EWorkshopFileTypeMicrotransaction},
                                   {"collection", k_EWorkshopFileTypeCollection},
                                   {"art", k_EWorkshopFileTypeArt},
                                   {"video", k_EWorkshopFileTypeVideo},
                                   {"screenshot", k_EWorkshopFileTypeScreenshot},
                                   {0, 0}};

static luax::Enum g_steamResultCodes[] = {{"OK", 1},
                                          {"Fail", 2},
                                          {"NoConnection", 3},
                                          {"InvalidPassword", 5},
                                          {"LoggedInElsewhere", 6},
                                          {"InvalidProtocolVer", 7},
                                          {"InvalidParam", 8},
                                          {"FileNotFound", 9},
                                          {"Busy", 10},
                                          {"InvalidState", 11},
                                          {"InvalidName", 12},
                                          {"InvalidEmail", 13},
                                          {"DuplicateName", 14},
                                          {"AccessDenied", 15},
                                          {"Timeout", 16},
                                          {"Banned", 17},
                                          {"AccountNotFound", 18},
                                          {"InvalidSteamID", 19},
                                          {"ServiceUnavailable", 20},
                                          {"NotLoggedOn", 21},
                                          {"Pending", 22},
                                          {"EncryptionFailure", 23},
                                          {"InsufficientPrivilege", 24},
                                          {"LimitExceeded", 25},
                                          {"Revoked", 26},
                                          {"Expired", 27},
                                          {"AlreadyRedeemed", 28},
                                          {"DuplicateRequest", 29},
                                          {"AlreadyOwned", 30},
                                          {"IPNotFound", 31},
                                          {"PersistFailed", 32},
                                          {"LockingFailed", 33},
                                          {"LogonSessionReplaced", 34},
                                          {"ConnectFailed", 35},
                                          {"HandshakeFailed", 36},
                                          {"IOFailure", 37},
                                          {"RemoteDisconnect", 38},
                                          {"ShoppingCartNotFound", 39},
                                          {"Blocked", 40},
                                          {"Ignored", 41},
                                          {"NoMatch", 42},
                                          {"AccountDisabled", 43},
                                          {"ServiceReadOnly", 44},
                                          {"AccountNotFeatured", 45},
                                          {"AdministratorOK", 46},
                                          {"ContentVersion", 47},
                                          {"TryAnotherCM", 48},
                                          {"PasswordRequiredToKickSession", 49},
                                          {"AlreadyLoggedInElsewhere", 50},
                                          {"Suspended", 51},
                                          {"Cancelled", 52},
                                          {"DataCorruption", 53},
                                          {"DiskFull", 54},
                                          {"RemoteCallFailed", 55},
                                          {"PasswordUnset", 56},
                                          {"ExternalAccountUnlinked", 57},
                                          {"PSNTicketInvalid", 58},
                                          {"ExternalAccountAlreadyLinked", 59},
                                          {"RemoteFileConflict", 60},
                                          {"IllegalPassword", 61},
                                          {"SameAsPreviousValue", 62},
                                          {"AccountLogonDenied", 63},
                                          {"CannotUseOldPassword", 64},
                                          {"InvalidLoginAuthCode", 65},
                                          {"AccountLogonDeniedNoMail", 66},
                                          {"HardwareNotCapableOfIPT", 67},
                                          {"IPTInitError", 68},
                                          {"ParentalControlRestricted", 69},
                                          {"FacebookQueryError", 70},
                                          {"ExpiredLoginAuthCode", 71},
                                          {"IPLoginRestrictionFailed", 72},
                                          {"AccountLockedDown", 73},
                                          {"AccountLogonDeniedVerifiedEmailRequired", 74},
                                          {"NoMatchingURL", 75},
                                          {0, 0}};

// ---- SteamContext ----------------------------------------------------------------

// 0x08131430: registers the callbacks (in member order) and caches the interfaces.
SteamContext::SteamContext()
    : m_userStatsReceived(this, &SteamContext::OnUserStatsReceived),
      m_userStatsStored(this, &SteamContext::OnUserStatsStored),
      m_achievementStored(this, &SteamContext::OnAchievementStored),
      m_personaStateChanged(this, &SteamContext::OnPersonaStateChanged),
      m_fileSubscribed(this, &SteamContext::OnFileSubscribed),
      m_fileUnsubscribed(this, &SteamContext::OnFileUnsubscribed)
{
    m_gameId = CGameID(SteamUtils()->GetAppID());
    m_pUserStats = SteamUserStats();
    m_pRemoteStorage = SteamRemoteStorage();
}

// 0x08132040: drops the events Lua did not poll during the frame and runs the callbacks,
// which queue this frame's events.
void SteamContext::update()
{
    m_events.resize(0);
    SteamAPI_RunCallbacks();
}

// 0x08131f30
void SteamContext::OnUserStatsReceived(UserStatsReceived_t* pParam)
{
    if (pParam->m_nGameID == m_gameId.ToUint64())
    {
        SteamEvent& e = m_events.push_back();
        e.type = SteamEvent::UserStatsReceived;
        e.statsReceived.result = pParam->m_eResult;
    }
}

// 0x08131e10
void SteamContext::OnUserStatsStored(UserStatsStored_t* pParam)
{
    if (pParam->m_nGameID == m_gameId.ToUint64())
    {
        SteamEvent& e = m_events.push_back();
        e.type = SteamEvent::UserStatsStored;
        e.statsStored = *pParam;
    }
}

// 0x08133020
void SteamContext::OnAchievementStored(UserAchievementStored_t* pParam)
{
    if (pParam->m_nGameID == m_gameId.ToUint64())
    {
        SteamEvent& e = m_events.push_back();
        e.type = SteamEvent::AchievementStored;
        e.achievementStored = *pParam;
    }
}

// 0x081307d0
void SteamContext::OnPersonaStateChanged(PersonaStateChange_t* pParam) {}

// 0x08132770
void SteamContext::OnFileSubscribed(RemoteStoragePublishedFileSubscribed_t* pParam)
{
    if ((uint64)pParam->m_nAppID == m_gameId.ToUint64())
    {
        SteamEvent& e = m_events.push_back();
        e.type = SteamEvent::FileSubscribed;
        e.fileSubscribed = *pParam;
    }
}

// 0x08133e20
void SteamContext::OnFileUnsubscribed(RemoteStoragePublishedFileUnsubscribed_t* pParam)
{
    if ((uint64)pParam->m_nAppID == m_gameId.ToUint64())
    {
        SteamEvent& e = m_events.push_back();
        e.type = SteamEvent::FileUnsubscribed;
        e.fileUnsubscribed = *pParam;
    }
}

// 0x08131410
extern "C" void SteamAPIDebugTextHook(int nSeverity, const char* pchDebugText)
{
    core::debugPrint("%s", pchDebugText);
}

// 0x08131220
void shutdownSteam()
{
    SteamAPI_Shutdown();
    delete g_pSteamContext; // the CCallback members unregister themselves
    g_pSteamContext = 0;
}

// Tags argument of the workshop functions: nil or an array of strings. The caller owns
// the string array (the original never frees it).
static SteamParamStringArray_t getTagArray(lua_State* L, int index)
{
    SteamParamStringArray_t tags;
    tags.m_ppStrings = 0;
    tags.m_nNumStrings = 0;
    if (lua_type(L, index) != LUA_TNONE)
    {
        luaL_checktype(L, index, LUA_TTABLE);
        tags.m_nNumStrings = (int)lua_objlen(L, index);
        tags.m_ppStrings = new const char*[tags.m_nNumStrings];
        lua_pushnil(L);
        int i = 0;
        while (lua_next(L, index))
        {
            if (!lua_isstring(L, -1))
                luaL_error(L, "invalid tag");
            tags.m_ppStrings[i++] = lua_tostring(L, -1);
            lua_pop(L, 1);
        }
    }
    return tags;
}

// ---- module functions ------------------------------------------------------------

// 0x081316f0: Steam.init(appId) -> true, or nil + "restart" / "SteamAPI_Init() failed"
static int Steam_init(lua_State* L)
{
    uint32 appId = (uint32)luaL_checkinteger(L, 1);
    if (!g_pSteamContext)
    {
        if (SteamAPI_RestartAppIfNecessary(appId))
        {
            lua_pushnil(L);
            lua_pushstring(L, "restart");
            return 2;
        }
        if (!SteamAPI_Init())
        {
            lua_pushnil(L);
            lua_pushstring(L, "SteamAPI_Init() failed");
            return 2;
        }
        SteamClient()->SetWarningMessageHook(SteamAPIDebugTextHook);
        g_pSteamContext = new SteamContext();
    }
    lua_pushboolean(L, 1);
    return 1;
}

// 0x08132270
static int Steam_update(lua_State* L)
{
    g_pSteamContext->update();
    return 0;
}

// 0x081323b0: pops the oldest event as a table {type=..., ...}, nil when there is none.
static int Steam_pollEvents(lua_State* L)
{
    if (g_pSteamContext->m_events.size() == 0)
    {
        lua_pushnil(L);
        return 1;
    }
    SteamEvent e = g_pSteamContext->m_events[0];
    lua_createtable(L, 0, 0);
    luax::Enum types[] = {{"userStatsReceived", SteamEvent::UserStatsReceived},
                          {"userStatsStored", SteamEvent::UserStatsStored},
                          {"achievementStored", SteamEvent::AchievementStored},
                          {"fileSubscribed", SteamEvent::FileSubscribed},
                          {"fileUnsubscribed", SteamEvent::FileUnsubscribed},
                          {0, 0}};
    luax::pushEnum(L, e.type, types);
    lua_setfield(L, -2, "type");
    switch (e.type)
    {
    case SteamEvent::UserStatsReceived:
        luax::pushEnum(L, e.statsReceived.result, g_steamResultCodes);
        lua_setfield(L, -2, "result");
        break;
    case SteamEvent::UserStatsStored:
        luax::pushEnum(L, e.statsStored.m_eResult, g_steamResultCodes);
        lua_setfield(L, -2, "result");
        break;
    case SteamEvent::AchievementStored:
        lua_pushstring(L, e.achievementStored.m_rgchAchievementName);
        lua_setfield(L, -2, "achievementName");
        lua_pushnumber(L, e.achievementStored.m_nCurProgress);
        lua_setfield(L, -2, "curProgress");
        lua_pushnumber(L, e.achievementStored.m_nMaxProgress);
        lua_setfield(L, -2, "maxProgress");
        break;
    case SteamEvent::FileSubscribed:
    case SteamEvent::FileUnsubscribed:
        luax::pushUInt64(L, e.fileSubscribed.m_nPublishedFileId);
        lua_setfield(L, -2, "publishedFileId");
        break;
    default:
        luaL_error(L, "invalid steam event type");
    }
    g_pSteamContext->m_events.erase(0);
    return 1;
}

// 0x08131130
static int Steam_setOverlayNotificationPosition(lua_State* L)
{
    luax::Enum positions[] = {{"top_left", k_EPositionTopLeft},
                              {"top_right", k_EPositionTopRight},
                              {"bottom_left", k_EPositionBottomLeft},
                              {"bottom_right", k_EPositionBottomRight},
                              {0, 0}};
    ENotificationPosition position = (ENotificationPosition)luax::checkEnum(L, 1, positions);
    SteamUtils()->SetOverlayNotificationPosition(position);
    return 0;
}

// ---- stats and achievements ------------------------------------------------------

// 0x08130ea0: ISteamUserStats::RequestCurrentStats() was removed from the SDK (stats are
// requested by SteamAPI_Init and UserStatsReceived_t is still delivered), so the request
// is reported as issued.
static int Steam_requestCurrentStats(lua_State* L)
{
    lua_pushboolean(L, 1);
    return 1;
}
// 0x08130860
static int Steam_setAchievement(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    g_pSteamContext->m_pUserStats->SetAchievement(name);
    return 0;
}
// 0x08130e30
static int Steam_getAchievement(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    bool achieved;
    g_pSteamContext->m_pUserStats->GetAchievement(name, &achieved);
    lua_pushboolean(L, achieved);
    return 1;
}
// 0x081308e0
static int Steam_getAchievementDisplayAttribute(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const char* key = luaL_checkstring(L, 2);
    lua_pushstring(L, g_pSteamContext->m_pUserStats->GetAchievementDisplayAttribute(name, key));
    return 1;
}
// 0x08130df0
static int Steam_storeStats(lua_State* L)
{
    lua_pushboolean(L, g_pSteamContext->m_pUserStats->StoreStats());
    return 1;
}
// 0x08130db0
static int Steam_resetAllStats(lua_State* L)
{
    lua_pushboolean(L, g_pSteamContext->m_pUserStats->ResetAllStats(true));
    return 1;
}

// ---- cloud storage ---------------------------------------------------------------

// 0x08130ee0
static int Steam_fileWrite(lua_State* L)
{
    const char* filename = luaL_checkstring(L, 1);
    const char* data = luaL_checkstring(L, 2);
    int size = (int)lua_objlen(L, 2);
    lua_pushboolean(L, g_pSteamContext->m_pRemoteStorage->FileWrite(filename, data, size));
    return 1;
}
// 0x08130960
static int Steam_fileRead(lua_State* L)
{
    const char* filename = luaL_checkstring(L, 1);
    int size = g_pSteamContext->m_pRemoteStorage->GetFileSize(filename);
    if (size == 0)
        luaL_error(L, "file not found: %s", filename);
    char* buffer = new char[size];
    int read = g_pSteamContext->m_pRemoteStorage->FileRead(filename, buffer, size);
    lua_pushlstring(L, buffer, read);
    delete[] buffer;
    return 1;
}
// 0x08130d50
static int Steam_fileForget(lua_State* L)
{
    const char* filename = luaL_checkstring(L, 1);
    lua_pushboolean(L, g_pSteamContext->m_pRemoteStorage->FileForget(filename));
    return 1;
}
// 0x08130cf0
static int Steam_fileDelete(lua_State* L)
{
    const char* filename = luaL_checkstring(L, 1);
    lua_pushboolean(L, g_pSteamContext->m_pRemoteStorage->FileDelete(filename));
    return 1;
}
// 0x08130c90
static int Steam_fileExists(lua_State* L)
{
    const char* filename = luaL_checkstring(L, 1);
    lua_pushboolean(L, g_pSteamContext->m_pRemoteStorage->FileExists(filename));
    return 1;
}
// 0x08130c30
static int Steam_filePersisted(lua_State* L)
{
    const char* filename = luaL_checkstring(L, 1);
    lua_pushboolean(L, g_pSteamContext->m_pRemoteStorage->FilePersisted(filename));
    return 1;
}
// 0x08130ae0
static int Steam_getFileSize(lua_State* L)
{
    const char* filename = luaL_checkstring(L, 1);
    lua_pushnumber(L, g_pSteamContext->m_pRemoteStorage->GetFileSize(filename));
    return 1;
}
// 0x08130aa0
static int Steam_getFileCount(lua_State* L)
{
    lua_pushnumber(L, g_pSteamContext->m_pRemoteStorage->GetFileCount());
    return 1;
}
// 0x08130b40: getFileNameAndSize(index) -> name, size (1-based index)
static int Steam_getFileNameAndSize(lua_State* L)
{
    int index = (int)luaL_checkinteger(L, 1);
    int32 size;
    const char* name = g_pSteamContext->m_pRemoteStorage->GetFileNameAndSize(index - 1, &size);
    lua_pushstring(L, name);
    lua_pushnumber(L, size);
    return 2;
}
// 0x08130a30: getQuota() -> total, available
static int Steam_getQuota(lua_State* L)
{
    uint64 total = 0, available = 0;
    g_pSteamContext->m_pRemoteStorage->GetQuota(&total, &available);
    lua_pushnumber(L, (lua_Number)total);
    lua_pushnumber(L, (lua_Number)available);
    return 2;
}
// 0x08130bf0
static int Steam_isCloudEnabledForAccount(lua_State* L)
{
    lua_pushboolean(L, g_pSteamContext->m_pRemoteStorage->IsCloudEnabledForAccount());
    return 1;
}
// 0x08130bb0
static int Steam_isCloudEnabledForApp(lua_State* L)
{
    lua_pushboolean(L, g_pSteamContext->m_pRemoteStorage->IsCloudEnabledForApp());
    return 1;
}
// 0x081311c0
static int Steam_setCloudEnabledForApp(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TBOOLEAN);
    g_pSteamContext->m_pRemoteStorage->SetCloudEnabledForApp(lua_toboolean(L, 1) != 0);
    return 0;
}

// ---- asynchronous call results ---------------------------------------------------

// 0x08131080: getAPICallStatus(handle) -> "failed" | "completed" | "pending"
static int Steam_getAPICallStatus(lua_State* L)
{
    SteamAPICall_t handle = luax::checkUInt64(L, 1);
    bool failed = false;
    bool completed = SteamUtils()->IsAPICallCompleted(handle, &failed);
    if (failed)
        lua_pushstring(L, "failed");
    else if (completed)
        lua_pushstring(L, "completed");
    else
        lua_pushstring(L, "pending");
    return 1;
}

// 0x08132170
static int Steam_getAPICallFailureReason(lua_State* L)
{
    luax::Enum reasons[] = {{"None", k_ESteamAPICallFailureNone},
                            {"SteamGone", k_ESteamAPICallFailureSteamGone},
                            {"NetworkFailure", k_ESteamAPICallFailureNetworkFailure},
                            {"InvalidHandle", k_ESteamAPICallFailureInvalidHandle},
                            {"MismatchedCallback", k_ESteamAPICallFailureMismatchedCallback},
                            {0, 0}};
    SteamAPICall_t handle = luax::checkUInt64(L, 1);
    luax::pushEnum(L, SteamUtils()->GetAPICallFailureReason(handle), reasons);
    return 1;
}

template <class T> static void getCallResult(lua_State* L, SteamAPICall_t handle, T& result)
{
    bool failed = false;
    if (!SteamUtils()->GetAPICallResult(handle, &result, sizeof(T), T::k_iCallback, &failed) ||
        failed)
        luaL_error(L, "GetAPICallResult failed");
}

static void pushPublishedFileIds(lua_State* L, const PublishedFileId_t* ids, int count, int total)
{
    lua_createtable(L, 0, 0);
    for (int i = 0; i < count; ++i)
    {
        luax::pushUInt64(L, ids[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "publishedFileIds");
    lua_pushnumber(L, total);
    lua_setfield(L, -2, "totalResultCount");
}

// 0x08133140: getAPICallResult(handle, resultType) -> table
static int Steam_getAPICallResult(lua_State* L)
{
    enum
    {
        PublishFile = 1,
        UpdatePublishedFile,
        DeletePublishedFile,
        GetPublishedFileDetails,
        EnumerateUserPublishedFiles,
        EnumerateUserSubscribedFiles,
        DownloadUGC
    };
    luax::Enum resultTypes[] = {
        {"RemoteStoragePublishFileResult", PublishFile},
        {"RemoteStorageUpdatePublishedFileResult", UpdatePublishedFile},
        {"RemoteStorageDeletePublishedFileResult", DeletePublishedFile},
        {"RemoteStorageGetPublishedFileDetailsResult", GetPublishedFileDetails},
        {"RemoteStorageEnumerateUserPublishedFilesResult", EnumerateUserPublishedFiles},
        {"RemoteStorageEnumerateUserSubscribedFilesResult", EnumerateUserSubscribedFiles},
        {"RemoteStorageDownloadUGCResult", DownloadUGC},
        {0, 0}};
    SteamAPICall_t handle = luax::checkUInt64(L, 1);
    int type = luax::checkEnum(L, 2, resultTypes);
    switch (type)
    {
    case PublishFile:
    {
        RemoteStoragePublishFileResult_t r;
        getCallResult(L, handle, r);
        lua_createtable(L, 0, 0);
        luax::pushEnum(L, r.m_eResult, g_steamResultCodes);
        lua_setfield(L, -2, "result");
        if (r.m_eResult == k_EResultOK)
        {
            luax::pushUInt64(L, r.m_nPublishedFileId);
            lua_setfield(L, -2, "publishedFileId");
        }
        return 1;
    }
    case UpdatePublishedFile:
    {
        RemoteStorageUpdatePublishedFileResult_t r;
        getCallResult(L, handle, r);
        lua_createtable(L, 0, 0);
        luax::pushEnum(L, r.m_eResult, g_steamResultCodes);
        lua_setfield(L, -2, "result");
        if (r.m_eResult == k_EResultOK)
        {
            luax::pushUInt64(L, r.m_nPublishedFileId);
            lua_setfield(L, -2, "publishedFileId");
        }
        return 1;
    }
    case DeletePublishedFile:
    {
        RemoteStorageDeletePublishedFileResult_t r;
        getCallResult(L, handle, r);
        lua_createtable(L, 0, 0);
        luax::pushEnum(L, r.m_eResult, g_steamResultCodes);
        lua_setfield(L, -2, "result");
        if (r.m_eResult == k_EResultOK)
        {
            luax::pushUInt64(L, r.m_nPublishedFileId);
            lua_setfield(L, -2, "publishedFileId");
        }
        return 1;
    }
    case GetPublishedFileDetails:
    {
        RemoteStorageGetPublishedFileDetailsResult_t r;
        getCallResult(L, handle, r);
        lua_createtable(L, 0, 0);
        luax::pushEnum(L, r.m_eResult, g_steamResultCodes);
        lua_setfield(L, -2, "result");
        if (r.m_eResult != k_EResultOK)
            return 1;
        luax::pushUInt64(L, r.m_nPublishedFileId);
        lua_setfield(L, -2, "publishedFileId");
        lua_pushnumber(L, r.m_nCreatorAppID);
        lua_setfield(L, -2, "creatorAppID");
        lua_pushnumber(L, r.m_nConsumerAppID);
        lua_setfield(L, -2, "consumerAppID");
        lua_pushstring(L, r.m_rgchTitle);
        lua_setfield(L, -2, "title");
        lua_pushstring(L, r.m_rgchDescription);
        lua_setfield(L, -2, "description");
        luax::pushUInt64(L, r.m_hFile);
        lua_setfield(L, -2, "fileHandle");
        luax::pushUInt64(L, r.m_hPreviewFile);
        lua_setfield(L, -2, "previewFileHandle");
        luax::pushUInt64(L, r.m_ulSteamIDOwner);
        lua_setfield(L, -2, "steamIDOwner");
        lua_pushnumber(L, r.m_rtimeCreated);
        lua_setfield(L, -2, "timeCreated");
        lua_pushnumber(L, r.m_rtimeUpdated);
        lua_setfield(L, -2, "timeUpdated");
        luax::pushEnum(L, r.m_eVisibility, g_visibilities);
        lua_setfield(L, -2, "visibility");
        lua_pushboolean(L, r.m_bBanned);
        lua_setfield(L, -2, "banned");
        lua_pushstring(L, r.m_rgchTags);
        lua_setfield(L, -2, "tags");
        lua_pushboolean(L, r.m_bTagsTruncated);
        lua_setfield(L, -2, "tagsTruncated");
        lua_pushstring(L, r.m_pchFileName);
        lua_setfield(L, -2, "filename");
        lua_pushnumber(L, r.m_nFileSize);
        lua_setfield(L, -2, "fileSize");
        lua_pushnumber(L, r.m_nPreviewFileSize);
        lua_setfield(L, -2, "previewFileSize");
        lua_pushstring(L, r.m_rgchURL);
        lua_setfield(L, -2, "URL");
        return 1;
    }
    case EnumerateUserPublishedFiles:
    {
        RemoteStorageEnumerateUserPublishedFilesResult_t r;
        getCallResult(L, handle, r);
        lua_createtable(L, 0, 0);
        luax::pushEnum(L, r.m_eResult, g_steamResultCodes);
        lua_setfield(L, -2, "result");
        if (r.m_eResult != k_EResultOK)
            return 1;
        pushPublishedFileIds(L, r.m_rgPublishedFileId, r.m_nResultsReturned, r.m_nTotalResultCount);
        return 1;
    }
    case EnumerateUserSubscribedFiles:
    {
        RemoteStorageEnumerateUserSubscribedFilesResult_t r;
        getCallResult(L, handle, r);
        lua_createtable(L, 0, 0);
        luax::pushEnum(L, r.m_eResult, g_steamResultCodes);
        lua_setfield(L, -2, "result");
        if (r.m_eResult != k_EResultOK)
            return 1;
        pushPublishedFileIds(L, r.m_rgPublishedFileId, r.m_nResultsReturned, r.m_nTotalResultCount);
        return 1;
    }
    case DownloadUGC:
    {
        RemoteStorageDownloadUGCResult_t r;
        getCallResult(L, handle, r);
        lua_createtable(L, 0, 0);
        luax::pushEnum(L, r.m_eResult, g_steamResultCodes);
        lua_setfield(L, -2, "result");
        if (r.m_eResult != k_EResultOK)
            return 1;
        luax::pushUInt64(L, r.m_hFile);
        lua_setfield(L, -2, "file");
        lua_pushnumber(L, r.m_nAppID);
        lua_setfield(L, -2, "appID");
        lua_pushnumber(L, r.m_nSizeInBytes);
        lua_setfield(L, -2, "sizeInBytes");
        lua_pushstring(L, r.m_pchFileName);
        lua_setfield(L, -2, "filename");
        luax::pushUInt64(L, r.m_ulSteamIDOwner);
        lua_setfield(L, -2, "steamIDOwner");
        return 1;
    }
    default:
        luaL_error(L, "invalid call result type");
        return 1;
    }
}

// ---- workshop --------------------------------------------------------------------

// 0x08132e90: publishWorkshopFile(file, previewFile, consumerAppId, title, description,
// visibility, tags, fileType) -> call handle
static int Steam_publishWorkshopFile(lua_State* L)
{
    const char* file = luaL_checkstring(L, 1);
    const char* previewFile = 0;
    if (lua_type(L, 2) != LUA_TNONE)
        previewFile = luaL_checkstring(L, 2);
    AppId_t consumerAppId = (AppId_t)luaL_checkinteger(L, 3);
    const char* title = luaL_checkstring(L, 4);
    const char* description = luaL_checkstring(L, 5);
    ERemoteStoragePublishedFileVisibility visibility =
        (ERemoteStoragePublishedFileVisibility)luax::checkEnum(L, 6, g_visibilities);
    EWorkshopFileType fileType = (EWorkshopFileType)luax::checkEnum(L, 8, g_fileTypes);
    SteamParamStringArray_t tags = getTagArray(L, 7);
    SteamAPICall_t call = g_pSteamContext->m_pRemoteStorage->PublishWorkshopFile(
        file, previewFile, consumerAppId, title, description, visibility, &tags, fileType);
    delete[] tags.m_ppStrings;
    luax::pushUInt64(L, call);
    return 1;
}
// 0x08132cc0
static int Steam_createPublishedFileUpdateRequest(lua_State* L)
{
    PublishedFileId_t id = luax::checkUInt64(L, 1);
    luax::pushUInt64(L, g_pSteamContext->m_pRemoteStorage->CreatePublishedFileUpdateRequest(id));
    return 1;
}
// 0x08131c60
static int Steam_updatePublishedFileFile(lua_State* L)
{
    PublishedFileUpdateHandle_t handle = luax::checkUInt64(L, 1);
    const char* file = luaL_checkstring(L, 2);
    lua_pushboolean(L, g_pSteamContext->m_pRemoteStorage->UpdatePublishedFileFile(handle, file));
    return 1;
}
// 0x08131bd0
static int Steam_updatePublishedFilePreviewFile(lua_State* L)
{
    PublishedFileUpdateHandle_t handle = luax::checkUInt64(L, 1);
    const char* file = luaL_checkstring(L, 2);
    lua_pushboolean(
        L, g_pSteamContext->m_pRemoteStorage->UpdatePublishedFilePreviewFile(handle, file));
    return 1;
}
// 0x08131b40
static int Steam_updatePublishedFileTitle(lua_State* L)
{
    PublishedFileUpdateHandle_t handle = luax::checkUInt64(L, 1);
    const char* title = luaL_checkstring(L, 2);
    lua_pushboolean(L, g_pSteamContext->m_pRemoteStorage->UpdatePublishedFileTitle(handle, title));
    return 1;
}
// 0x08131ab0
static int Steam_updatePublishedFileDescription(lua_State* L)
{
    PublishedFileUpdateHandle_t handle = luax::checkUInt64(L, 1);
    const char* description = luaL_checkstring(L, 2);
    lua_pushboolean(
        L, g_pSteamContext->m_pRemoteStorage->UpdatePublishedFileDescription(handle, description));
    return 1;
}
// 0x08131d80
static int Steam_updatePublishedFileVisibility(lua_State* L)
{
    PublishedFileUpdateHandle_t handle = luax::checkUInt64(L, 1);
    ERemoteStoragePublishedFileVisibility visibility =
        (ERemoteStoragePublishedFileVisibility)luax::checkEnum(L, 2, g_visibilities);
    lua_pushboolean(
        L, g_pSteamContext->m_pRemoteStorage->UpdatePublishedFileVisibility(handle, visibility));
    return 1;
}
// 0x08131cf0
static int Steam_updatePublishedFileTags(lua_State* L)
{
    PublishedFileUpdateHandle_t handle = luax::checkUInt64(L, 1);
    SteamParamStringArray_t tags = getTagArray(L, 2);
    bool ok = g_pSteamContext->m_pRemoteStorage->UpdatePublishedFileTags(handle, &tags);
    delete[] tags.m_ppStrings;
    lua_pushboolean(L, ok);
    return 1;
}
// 0x08132c10
static int Steam_commitPublishedFileUpdate(lua_State* L)
{
    PublishedFileUpdateHandle_t handle = luax::checkUInt64(L, 1);
    luax::pushUInt64(L, g_pSteamContext->m_pRemoteStorage->CommitPublishedFileUpdate(handle));
    return 1;
}
// 0x08132b60
static int Steam_deletePublishedFile(lua_State* L)
{
    PublishedFileId_t id = luax::checkUInt64(L, 1);
    luax::pushUInt64(L, g_pSteamContext->m_pRemoteStorage->DeletePublishedFile(id));
    return 1;
}
// 0x08132ab0
static int Steam_getPublishedFileDetails(lua_State* L)
{
    PublishedFileId_t id = luax::checkUInt64(L, 1);
    luax::pushUInt64(L, g_pSteamContext->m_pRemoteStorage->GetPublishedFileDetails(id, 0));
    return 1;
}
// 0x08132a00
static int Steam_subscribePublishedFile(lua_State* L)
{
    PublishedFileId_t id = luax::checkUInt64(L, 1);
    luax::pushUInt64(L, g_pSteamContext->m_pRemoteStorage->SubscribePublishedFile(id));
    return 1;
}
// 0x08132950
static int Steam_unsubscribePublishedFile(lua_State* L)
{
    PublishedFileId_t id = luax::checkUInt64(L, 1);
    luax::pushUInt64(L, g_pSteamContext->m_pRemoteStorage->UnsubscribePublishedFile(id));
    return 1;
}
// 0x08132e00
static int Steam_enumerateUserPublishedFiles(lua_State* L)
{
    uint32 startIndex = (uint32)luaL_checkinteger(L, 1);
    luax::pushUInt64(L, g_pSteamContext->m_pRemoteStorage->EnumerateUserPublishedFiles(startIndex));
    return 1;
}
// 0x08132d70
static int Steam_enumerateUserSubscribedFiles(lua_State* L)
{
    uint32 startIndex = (uint32)luaL_checkinteger(L, 1);
    luax::pushUInt64(L,
                     g_pSteamContext->m_pRemoteStorage->EnumerateUserSubscribedFiles(startIndex));
    return 1;
}
// 0x08132890
static int Steam_UGCDownload(lua_State* L)
{
    UGCHandle_t handle = luax::checkUInt64(L, 1);
    luax::pushUInt64(L, g_pSteamContext->m_pRemoteStorage->UGCDownload(handle, 0));
    return 1;
}
// 0x08131a10: UGCDownloadProgress(handle) -> downloaded, expected
static int Steam_UGCDownloadProgress(lua_State* L)
{
    UGCHandle_t handle = luax::checkUInt64(L, 1);
    int32 downloaded = 0, expected = 0;
    g_pSteamContext->m_pRemoteStorage->GetUGCDownloadProgress(handle, &downloaded, &expected);
    lua_pushnumber(L, downloaded);
    lua_pushnumber(L, expected);
    return 2;
}
// 0x08131850: UGCRead(handle [, filename]): writes the content to filename, or returns it
// as a string (nil if the handle is unknown).
static int Steam_UGCRead(lua_State* L)
{
    UGCHandle_t handle = luax::checkUInt64(L, 1);
    const char* filename = 0;
    if (lua_gettop(L) > 1)
        filename = luaL_checkstring(L, 2);
    AppId_t appId;
    char* name;
    int32 size = 0;
    CSteamID owner;
    if (!g_pSteamContext->m_pRemoteStorage->GetUGCDetails(handle, &appId, &name, &size, &owner))
    {
        lua_pushnil(L);
        return 1;
    }
    char* buffer = new char[size];
    int read =
        g_pSteamContext->m_pRemoteStorage->UGCRead(handle, buffer, size, 0, k_EUGCRead_Close);
    if (filename)
    {
        File* file = core::openWrite(filename);
        file->write(buffer, read);
        core::closeFile(file);
        delete[] buffer; // the original leaks the buffer on this path
        return 0;
    }
    lua_pushlstring(L, buffer, read);
    delete[] buffer;
    return 1;
}

// ---- friends ---------------------------------------------------------------------

// 0x081317e0
static int Steam_getFriendPersonaName(lua_State* L)
{
    CSteamID id(luax::checkUInt64(L, 1));
    lua_pushstring(L, SteamFriends()->GetFriendPersonaName(id));
    return 1;
}
// 0x081308a0
static int Steam_activateGameOverlayToWebPage(lua_State* L)
{
    const char* url = luaL_checkstring(L, 1);
    SteamFriends()->ActivateGameOverlayToWebPage(url);
    return 0;
}

// 0x08130810
void steam_mod(lua_State* L)
{
    static const luaL_Reg functions[] = {
        {"init", Steam_init},
        {"update", Steam_update},
        {"pollEvents", Steam_pollEvents},
        {"setOverlayNotificationPosition", Steam_setOverlayNotificationPosition},
        {"requestCurrentStats", Steam_requestCurrentStats},
        {"setAchievement", Steam_setAchievement},
        {"getAchievement", Steam_getAchievement},
        {"getAchievementDisplayAttribute", Steam_getAchievementDisplayAttribute},
        {"storeStats", Steam_storeStats},
        {"resetAllStats", Steam_resetAllStats},
        {"fileWrite", Steam_fileWrite},
        {"fileRead", Steam_fileRead},
        {"fileForget", Steam_fileForget},
        {"fileDelete", Steam_fileDelete},
        {"fileExists", Steam_fileExists},
        {"filePersisted", Steam_filePersisted},
        {"getFileSize", Steam_getFileSize},
        {"getFileCount", Steam_getFileCount},
        {"getFileNameAndSize", Steam_getFileNameAndSize},
        {"getQuota", Steam_getQuota},
        {"isCloudEnabledForAccount", Steam_isCloudEnabledForAccount},
        {"isCloudEnabledForApp", Steam_isCloudEnabledForApp},
        {"setCloudEnabledForApp", Steam_setCloudEnabledForApp},
        {"getAPICallStatus", Steam_getAPICallStatus},
        {"getAPICallFailureReason", Steam_getAPICallFailureReason},
        {"getAPICallResult", Steam_getAPICallResult},
        {"publishWorkshopFile", Steam_publishWorkshopFile},
        {"createPublishedFileUpdateRequest", Steam_createPublishedFileUpdateRequest},
        {"updatePublishedFileFile", Steam_updatePublishedFileFile},
        {"updatePublishedFilePreviewFile", Steam_updatePublishedFilePreviewFile},
        {"updatePublishedFileTitle", Steam_updatePublishedFileTitle},
        {"updatePublishedFileDescription", Steam_updatePublishedFileDescription},
        {"updatePublishedFileVisibility", Steam_updatePublishedFileVisibility},
        {"updatePublishedFileTags", Steam_updatePublishedFileTags},
        {"commitPublishedFileUpdate", Steam_commitPublishedFileUpdate},
        {"deletePublishedFile", Steam_deletePublishedFile},
        {"getPublishedFileDetails", Steam_getPublishedFileDetails},
        {"subscribePublishedFile", Steam_subscribePublishedFile},
        {"unsubscribePublishedFile", Steam_unsubscribePublishedFile},
        {"enumerateUserPublishedFiles", Steam_enumerateUserPublishedFiles},
        {"enumerateUserSubscribedFiles", Steam_enumerateUserSubscribedFiles},
        {"UGCDownload", Steam_UGCDownload},
        {"UGCDownloadProgress", Steam_UGCDownloadProgress},
        {"UGCRead", Steam_UGCRead},
        {"getFriendPersonaName", Steam_getFriendPersonaName},
        {"activateGameOverlayToWebPage", Steam_activateGameOverlayToWebPage},
        {0, 0}};
    luax::registerModule(L, "Steam", functions);
}
