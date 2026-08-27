#include <algorithm>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "../steam/SteamCommon.h"
#include "../steam/RevCommon.h"
#define INCLUDED_STEAM2_USERID_STRUCTS
#include "../steam/Steam3ID.h"
#include "../steam/ClassicRevEmuTicket.h"

#if defined(__GNUC__)
#define REVIVE_EXPORT extern "C" __attribute__((visibility("default")))
#else
#define REVIVE_EXPORT extern "C"
#endif

namespace
{

typedef int HSteamPipe;
typedef int HSteamUser;
typedef uint64 HSteamCall;

static const HSteamPipe kPipe = 1;
static const HSteamUser kUser = 1;
static const int kCallbackSteamServersConnected = 101;
static const int kCallbackGSClientApprove = 201;

struct CallbackMsg_t
{
    HSteamUser m_hSteamUser;
    int m_iCallback;
    uint8 *m_pubParam;
    int m_cubParam;
};

struct QueuedCallback
{
    HSteamUser user;
    int callback;
    std::vector<uint8> payload;
};

std::mutex g_mutex;
std::deque<QueuedCallback> g_callbacks;
bool g_callbackInFlight = false;
HSteamUser g_currentCallbackUser = 0;
bool g_loggedOn = false;
uint32 g_localIP = 0;
uint16 g_localPort = 0;
std::map<uint32, CSteamID> g_steam2Users;
uint32 g_botCounter = 1;

FILE *OpenLog()
{
    const char *env = std::getenv("REVIVE_STEAMCLIENT_LOG");
    const char *path = (env && *env) ? env : "revive_steamclient.log";
    return std::fopen(path, "a");
}

void Log(const char *fmt, ...)
{
    FILE *f = OpenLog();
    if (!f)
        return;

    char stamp[32];
    std::time_t now = std::time(NULL);
    std::tm tmv;
#if defined(_WIN32)
    gmtime_s(&tmv, &now);
#else
    gmtime_r(&now, &tmv);
#endif
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &tmv);
    std::fprintf(f, "%s ", stamp);

    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(f, fmt, ap);
    va_end(ap);
    std::fputc('\n', f);
    std::fclose(f);
}

void QueueCallback(int callback, const void *payload, size_t payloadSize)
{
    QueuedCallback item;
    item.user = kUser;
    item.callback = callback;
    if (payload && payloadSize)
    {
        const uint8 *p = static_cast<const uint8 *>(payload);
        item.payload.assign(p, p + payloadSize);
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    g_callbacks.push_back(item);
    Log("callback queued id=%d size=%u", callback, static_cast<unsigned>(payloadSize));
}

void QueueClientApprove(const CSteamID &steamID)
{
    const uint64 id = steamID.ConvertToUint64();
    QueueCallback(kCallbackGSClientApprove, &id, sizeof(id));
}

const char *Steam2String(const CSteamID &steamID, char *buf, size_t bufSize)
{
    const uint32 account = steamID.GetAccountID();
    std::snprintf(buf, bufSize, "STEAM_0:%u:%u", account & 1u, account >> 1);
    return buf;
}

CSteamID MakeFallbackUserID(uint32 ip)
{
    uint32 account = ip;
    if (account == 0)
        account = 2;
    return CSteamID(account, k_unSteamUserDefaultInstance, k_EUniversePublic, k_EAccountTypeIndividual);
}

CSteamID MakeServerID()
{
    uint32 account = (g_localIP ^ (static_cast<uint32>(g_localPort) << 16) ^ 0x52455649u);
    if (account == 0)
        account = 0x539;
    return CSteamID(account, 0, k_EUniversePublic, k_EAccountTypeAnonGameServer);
}

class CSteamGameServer002
{
public:
    virtual void LogOn();
    virtual void LogOff();
    virtual bool BLoggedOn();
    virtual void GSSetSpawnCount(uint32 count);
    virtual bool GSGetSteam2GetEncryptionKeyToSendToNewClient(void *key, uint32 *keySize, uint32 maxKeySize);
    virtual bool GSSendSteam2UserConnect(uint32 accountId, const void *rawKey, uint32 rawKeyLen,
                                 uint32 ip, uint16 port, const void *cookie, uint32 cookieLen);
    virtual bool GSSendSteam3UserConnect(CSteamID steamID, uint32 ip, const void *cookie, uint32 cookieLen);
    virtual bool GSRemoveUserConnect(uint32 accountId);
    virtual bool GSSendUserDisconnect(CSteamID steamID, uint32 accountId);
    virtual bool GSSendUserStatusResponse(CSteamID steamID, int secondsConnected, int secondsSinceLast);
    virtual void Obsolete_GSSetStatus(int players, uint32 serverFlags, int botPlayers, int maxPlayers,
                              int serverIP, int serverPort, const char *serverName,
                              const char *mapName, const char *gameDir, const char *version);
    virtual bool GSUpdateStatus(int players, int maxPlayers, int botPlayers, const char *serverName, const char *mapName);
    virtual bool BSecure();
    virtual CSteamID GetSteamID();
    virtual bool GSSetServerType(int serverMode, uint32 serverIP, uint32 serverFlags, uint32 gameAppId,
                         const char *gameDir, const char *version);
    virtual bool GSSetServerType2(int serverMode, uint32 serverIP, uint32 serverFlags,
                          uint16 gamePort, uint16 spectatorPort, uint16 queryPort,
                          const char *gameDir, const char *version, bool lanMode);
    virtual bool GSUpdateStatus2(int players, int maxPlayers, int botPlayers,
                         const char *serverName, const char *mapName, const char *spectatorServerName);
    virtual bool GSCreateUnauthenticatedUser(CSteamID *steamID);
    virtual bool GSSetUserData(CSteamID steamID, const char *playerName, uint32 score);
    virtual void GSUpdateSpectatorPort(uint16 spectatorPort);
    virtual void GSSetGameType(const char *gameType);
};

class CSteamUtils001
{
public:
    virtual uint32 GetSecondsSinceAppActive() { return 0; }
    virtual uint32 GetSecondsSinceComputerActive() { return 0; }
    virtual EUniverse GetConnectedUniverse() { return k_EUniversePublic; }
    virtual uint32 GetServerRealTime() { return static_cast<uint32>(std::time(NULL)); }
    virtual const char *GetIPCountry() { return "US"; }
    virtual bool GetImageSize(int, uint32 *width, uint32 *height)
    {
        if (width) *width = 0;
        if (height) *height = 0;
        return false;
    }
    virtual bool GetImageRGBA(int, uint8 *, int) { return false; }
};

class CSteamMasterServerUpdater001
{
public:
    virtual void SetActive(bool active) { Log("MasterServerUpdater SetActive=%d", active ? 1 : 0); }
    virtual void SetHeartbeatInterval(int) {}
    virtual bool HandleIncomingPacket(const void *, int, uint32, uint16) { return false; }
    virtual int GetNextOutgoingPacket(void *, int, uint32 *, uint16 *) { return 0; }
    virtual void SetBasicServerData(uint16, bool, const char *, const char *, uint16, bool, const char *) {}
    virtual void ClearAllKeyValues() {}
    virtual void SetKeyValue(const char *, const char *) {}
    virtual void NotifyShutdown() {}
    virtual bool WasRestartRequested() { return false; }
    virtual void ForceHeartbeat() {}
    virtual bool AddMasterServer(const char *) { return false; }
    virtual bool RemoveMasterServer(const char *) { return false; }
    virtual int GetNumMasterServers() { return 0; }
    virtual int GetMasterServerAddress(int, char *address, int addressSize)
    {
        if (address && addressSize > 0) address[0] = '\0';
        return 0;
    }
};

CSteamGameServer002 g_gameServer;
CSteamUtils001 g_utils;
CSteamMasterServerUpdater001 g_masterUpdater;

class CSteamClient006
{
public:
    virtual HSteamPipe CreateSteamPipe() { return kPipe; }
    virtual bool BReleaseSteamPipe(HSteamPipe) { return true; }
    virtual HSteamUser CreateGlobalUser(HSteamPipe *pipe) { if (pipe) *pipe = kPipe; return kUser; }
    virtual HSteamUser ConnectToGlobalUser(HSteamPipe) { return kUser; }
    virtual HSteamUser CreateLocalUser(HSteamPipe *pipe) { if (pipe) *pipe = kPipe; Log("CreateLocalUser pipe=%d user=%d", kPipe, kUser); return kUser; }
    virtual void ReleaseUser(HSteamPipe, HSteamUser) {}
    virtual void *GetISteamUser(HSteamUser, HSteamPipe, const char *) { return NULL; }
    virtual void *GetIVAC(HSteamUser) { return NULL; }
    virtual void *GetISteamGameServer(HSteamUser, HSteamPipe, const char *version)
    {
        Log("GetISteamGameServer version=%s", version ? version : "(null)");
        return version && std::strcmp(version, "SteamGameServer002") == 0 ? &g_gameServer : NULL;
    }
    virtual void SetLocalIPBinding(uint32 ip, uint16 port)
    {
        g_localIP = ip;
        g_localPort = port;
        Log("SetLocalIPBinding ip=0x%08x port=%u", ip, static_cast<unsigned>(port));
    }
    virtual const char *GetUniverseName(EUniverse universe)
    {
        switch (universe)
        {
            case k_EUniversePublic: return "Public";
            case k_EUniverseBeta: return "Beta";
            case k_EUniverseInternal: return "Internal";
            case k_EUniverseDev: return "Dev";
            default: return "Invalid";
        }
    }
    virtual void *GetISteamFriends(HSteamUser, HSteamPipe, const char *) { return NULL; }
    virtual void *GetISteamUtils(HSteamPipe, const char *version)
    {
        Log("GetISteamUtils version=%s", version ? version : "(null)");
        return version && std::strcmp(version, "SteamUtils001") == 0 ? &g_utils : NULL;
    }
    virtual void *GetISteamBilling(HSteamUser, HSteamPipe, const char *) { return NULL; }
    virtual void *GetISteamMatchMaking(HSteamUser, HSteamPipe, const char *) { return NULL; }
    virtual void *GetISteamContentServer(HSteamUser, HSteamPipe, const char *) { return NULL; }
    virtual void *GetISteamApps(HSteamUser, HSteamPipe, const char *) { return NULL; }
    virtual void *GetISteamMasterServerUpdater(HSteamUser, HSteamPipe, const char *version)
    {
        Log("GetISteamMasterServerUpdater version=%s", version ? version : "(null)");
        return version && std::strcmp(version, "SteamMasterServerUpdater001") == 0 ? &g_masterUpdater : NULL;
    }
    virtual void *GetISteamMatchMakingServers(HSteamUser, HSteamPipe, const char *) { return NULL; }
    virtual void RunFrame() {}
    virtual uint32 GetIPCCallCount() { return 0; }
};

CSteamClient006 g_client;

void CSteamGameServer002::LogOn()
{
    if (!g_loggedOn)
    {
        g_loggedOn = true;
        Log("SteamGameServer002 LogOn");
        QueueCallback(kCallbackSteamServersConnected, NULL, 0);
    }
}

void CSteamGameServer002::LogOff()
{
    g_loggedOn = false;
    Log("SteamGameServer002 LogOff");
}

bool CSteamGameServer002::BLoggedOn()
{
    return g_loggedOn;
}

void CSteamGameServer002::GSSetSpawnCount(uint32 count)
{
    Log("GSSetSpawnCount count=%u", count);
}

bool CSteamGameServer002::GSGetSteam2GetEncryptionKeyToSendToNewClient(void *key, uint32 *keySize, uint32 maxKeySize)
{
    if (!key || !keySize || maxKeySize < sizeof(g_TicketKey))
    {
        if (keySize) *keySize = 0;
        Log("GSGetSteam2GetEncryptionKeyToSendToNewClient rejected max=%u", maxKeySize);
        return false;
    }

    std::memcpy(key, g_TicketKey, sizeof(g_TicketKey));
    *keySize = static_cast<uint32>(sizeof(g_TicketKey));
    Log("GSGetSteam2GetEncryptionKeyToSendToNewClient key_size=%u max=%u",
        *keySize, maxKeySize);
    return true;
}

bool CSteamGameServer002::GSSendSteam2UserConnect(uint32 accountId,
                                                  const void *rawKey,
                                                  uint32 rawKeyLen,
                                                  uint32 ip,
                                                  uint16 port,
                                                  const void *,
                                                  uint32 cookieLen)
{
    revive::ClassicRevEmuTicketInfo info;
    CSteamID steamID;
    bool classic = revive::ParseClassicRevEmuTicket(rawKey, rawKeyLen, &info);

    if (classic)
        steamID = info.steamID;
    else
        steamID = MakeFallbackUserID(ip);

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_steam2Users[accountId] = steamID;
    }

    char steam2[64];
    Log("GSSendSteam2UserConnect account=%u ip=0x%08x port=%u raw_len=%u cookie_len=%u type=%s steam2=%s steamid64=%llu",
        accountId, ip, static_cast<unsigned>(port), rawKeyLen, cookieLen,
        classic ? "ClassicRevEmu" : "FallbackIP",
        Steam2String(steamID, steam2, sizeof(steam2)),
        static_cast<unsigned long long>(steamID.ConvertToUint64()));

    if (classic)
        Log("ClassicRevEmu hash=%u steamid_low=0x%08x steamid_high=0x%08x",
            info.hash, info.steamIDLow, info.steamIDHigh);

    QueueClientApprove(steamID);
    return true;
}

bool CSteamGameServer002::GSSendSteam3UserConnect(CSteamID steamID, uint32 ip, const void *, uint32)
{
    char steam2[64];
    Log("GSSendSteam3UserConnect ip=0x%08x steam2=%s", ip,
        Steam2String(steamID, steam2, sizeof(steam2)));
    QueueClientApprove(steamID);
    return true;
}

bool CSteamGameServer002::GSRemoveUserConnect(uint32 accountId)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_steam2Users.erase(accountId);
    Log("GSRemoveUserConnect account=%u", accountId);
    return true;
}

bool CSteamGameServer002::GSSendUserDisconnect(CSteamID steamID, uint32 accountId)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_steam2Users.erase(accountId);
    Log("GSSendUserDisconnect account=%u steamid64=%llu", accountId,
        static_cast<unsigned long long>(steamID.ConvertToUint64()));
    return true;
}

bool CSteamGameServer002::GSSendUserStatusResponse(CSteamID, int, int)
{
    return true;
}

void CSteamGameServer002::Obsolete_GSSetStatus(int, uint32, int, int, int, int,
                                               const char *, const char *, const char *, const char *)
{
}

bool CSteamGameServer002::GSUpdateStatus(int, int, int, const char *, const char *)
{
    return true;
}

bool CSteamGameServer002::BSecure()
{
    return false;
}

CSteamID CSteamGameServer002::GetSteamID()
{
    return MakeServerID();
}

bool CSteamGameServer002::GSSetServerType(int serverMode, uint32 serverIP, uint32 serverFlags,
                                          uint32 gameAppId, const char *gameDir, const char *version)
{
    Log("GSSetServerType mode=%d ip=0x%08x flags=0x%08x app=%u dir=%s version=%s",
        serverMode, serverIP, serverFlags, gameAppId,
        gameDir ? gameDir : "", version ? version : "");
    return true;
}

bool CSteamGameServer002::GSSetServerType2(int serverMode, uint32 serverIP, uint32 serverFlags,
                                           uint16 gamePort, uint16 spectatorPort, uint16 queryPort,
                                           const char *gameDir, const char *version, bool lanMode)
{
    if (serverIP) g_localIP = serverIP;
    if (gamePort) g_localPort = gamePort;
    Log("GSSetServerType2 mode=%d ip=0x%08x flags=0x%08x game=%u spectator=%u query=%u dir=%s version=%s lan=%d",
        serverMode, serverIP, serverFlags,
        static_cast<unsigned>(gamePort), static_cast<unsigned>(spectatorPort), static_cast<unsigned>(queryPort),
        gameDir ? gameDir : "", version ? version : "", lanMode ? 1 : 0);
    return true;
}

bool CSteamGameServer002::GSUpdateStatus2(int, int, int, const char *, const char *, const char *)
{
    return true;
}

bool CSteamGameServer002::GSCreateUnauthenticatedUser(CSteamID *steamID)
{
    if (!steamID)
        return false;

    const uint32 account = 0xF0000000u | (g_botCounter++ & 0x0fffffffu);
    steamID->InstancedSet(account, 1, k_EUniversePublic, k_EAccountTypeAnonUser);
    return true;
}

bool CSteamGameServer002::GSSetUserData(CSteamID, const char *, uint32)
{
    return true;
}

void CSteamGameServer002::GSUpdateSpectatorPort(uint16) {}
void CSteamGameServer002::GSSetGameType(const char *) {}

} // namespace

REVIVE_EXPORT void *CreateInterface(const char *name, int *returnCode)
{
    void *result = NULL;

    if (name)
    {
        if (std::strcmp(name, "SteamClient006") == 0)
            result = &g_client;
        else if (std::strcmp(name, "SteamGameServer002") == 0)
            result = &g_gameServer;
        else if (std::strcmp(name, "SteamUtils001") == 0)
            result = &g_utils;
        else if (std::strcmp(name, "SteamMasterServerUpdater001") == 0)
            result = &g_masterUpdater;
    }

    if (returnCode)
        *returnCode = result ? 0 : 1;

    Log("CreateInterface name=%s result=%p", name ? name : "(null)", result);
    return result;
}

REVIVE_EXPORT bool Steam_BGetCallback(HSteamPipe pipe, CallbackMsg_t *msg, HSteamCall *call)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!msg || pipe != kPipe || g_callbackInFlight || g_callbacks.empty())
        return false;

    QueuedCallback &item = g_callbacks.front();
    msg->m_hSteamUser = item.user;
    msg->m_iCallback = item.callback;
    msg->m_pubParam = item.payload.empty() ? NULL : item.payload.data();
    msg->m_cubParam = static_cast<int>(item.payload.size());
    if (call) *call = 0;

    g_currentCallbackUser = item.user;
    g_callbackInFlight = true;
    Log("Steam_BGetCallback id=%d size=%d", msg->m_iCallback, msg->m_cubParam);
    return true;
}

REVIVE_EXPORT void Steam_FreeLastCallback(HSteamPipe pipe)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (pipe == kPipe && g_callbackInFlight && !g_callbacks.empty())
        g_callbacks.pop_front();
    g_callbackInFlight = false;
}

REVIVE_EXPORT HSteamUser Steam_GetHSteamUserCurrent()
{
    return g_currentCallbackUser ? g_currentCallbackUser : kUser;
}

REVIVE_EXPORT void Steam_RegisterInterfaceFuncs(void *) {}
REVIVE_EXPORT bool Steam_BConnected(HSteamUser, HSteamPipe) { return true; }
REVIVE_EXPORT bool Steam_BLoggedOn(HSteamUser, HSteamPipe) { return g_loggedOn; }
REVIVE_EXPORT HSteamPipe Steam_CreateSteamPipe() { return kPipe; }
REVIVE_EXPORT bool Steam_BReleaseSteamPipe(HSteamPipe) { return true; }
REVIVE_EXPORT HSteamUser Steam_CreateGlobalUser(HSteamPipe *pipe) { return g_client.CreateGlobalUser(pipe); }
REVIVE_EXPORT HSteamUser Steam_ConnectToGlobalUser(HSteamPipe pipe) { return g_client.ConnectToGlobalUser(pipe); }
REVIVE_EXPORT HSteamUser Steam_CreateLocalUser(HSteamPipe *pipe) { return g_client.CreateLocalUser(pipe); }
REVIVE_EXPORT void Steam_ReleaseUser(HSteamPipe pipe, HSteamUser user) { g_client.ReleaseUser(pipe, user); }
REVIVE_EXPORT void Steam_SetLocalIPBinding(uint32 ip, uint16 port) { g_client.SetLocalIPBinding(ip, port); }
REVIVE_EXPORT HSteamUser Steam_GetGSHandle() { return kUser; }
REVIVE_EXPORT void Steam_LogOn(HSteamUser, HSteamPipe, uint64) { g_loggedOn = true; }
REVIVE_EXPORT void Steam_LogOff(HSteamUser, HSteamPipe) { g_loggedOn = false; }
REVIVE_EXPORT bool Steam_GSBLoggedOn(HSteamUser, HSteamPipe) { return g_gameServer.BLoggedOn(); }
REVIVE_EXPORT bool Steam_GSBSecure(HSteamUser, HSteamPipe) { return g_gameServer.BSecure(); }
REVIVE_EXPORT void Steam_GSLogOn(HSteamUser, HSteamPipe) { g_gameServer.LogOn(); }
REVIVE_EXPORT void Steam_GSLogOff(HSteamUser, HSteamPipe) { g_gameServer.LogOff(); }
REVIVE_EXPORT void Steam_GSSetSpawnCount(HSteamUser, HSteamPipe, uint32 count) { g_gameServer.GSSetSpawnCount(count); }
REVIVE_EXPORT bool Steam_GSGetSteam2GetEncryptionKeyToSendToNewClient(HSteamUser, HSteamPipe, void *key, uint32 *size, uint32 maxSize)
{
    return g_gameServer.GSGetSteam2GetEncryptionKeyToSendToNewClient(key, size, maxSize);
}
REVIVE_EXPORT bool Steam_GSRemoveUserConnect(HSteamUser, HSteamPipe, uint32 accountId) { return g_gameServer.GSRemoveUserConnect(accountId); }
REVIVE_EXPORT bool Steam_GSSendUserDisconnect(HSteamUser, HSteamPipe, CSteamID id, uint32 accountId) { return g_gameServer.GSSendUserDisconnect(id, accountId); }
REVIVE_EXPORT bool Steam_GSSendUserStatusResponse(HSteamUser, HSteamPipe, CSteamID id, int a, int b) { return g_gameServer.GSSendUserStatusResponse(id, a, b); }
REVIVE_EXPORT CSteamID Steam_GSGetSteamID(HSteamUser, HSteamPipe) { return g_gameServer.GetSteamID(); }
REVIVE_EXPORT bool Steam_GSSetServerType(HSteamUser, HSteamPipe, int mode, uint32 ip, uint32 flags, uint32 appId, const char *dir, const char *version)
{
    return g_gameServer.GSSetServerType(mode, ip, flags, appId, dir, version);
}
REVIVE_EXPORT bool Steam_GSUpdateStatus(HSteamUser, HSteamPipe, int players, int maxPlayers, int bots, const char *name, const char *map)
{
    return g_gameServer.GSUpdateStatus(players, maxPlayers, bots, name, map);
}
REVIVE_EXPORT bool Steam_InitiateGameConnection(void *, int, CSteamID, uint32, uint16, bool) { return true; }
REVIVE_EXPORT void Steam_TerminateGameConnection(uint32, uint16) {}

// Marker used by Docker/startup validation without depending on symbol tools in runtime.
REVIVE_EXPORT const char *REVive_LegacySteamClient_BuildMarker()
{
    return "REVive legacy SteamClient006 backend M2.1";
}
