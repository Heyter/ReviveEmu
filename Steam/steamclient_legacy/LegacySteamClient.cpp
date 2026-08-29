#include <cstring>

#include "LegacySteamCallbacks.h"
#include "LegacySteamInterfaces.h"
#include "LegacySteamRuntime.h"

#if defined(__GNUC__)
#define REVIVE_EXPORT extern "C" __attribute__((visibility("default")))
#else
#define REVIVE_EXPORT extern "C"
#endif

using revive::legacy::CSteamClient006;
using revive::legacy::CSteamGameServer002;
using revive::legacy::CallbackMsg_t;
using revive::legacy::FreeLastCallback;
using revive::legacy::GameServerFromHandle;
using revive::legacy::GetNextCallback;
using revive::legacy::HSteamPipe;
using revive::legacy::HSteamUser;
using revive::legacy::Log;
using revive::legacy::Runtime;
using revive::legacy::SteamClient;
using revive::legacy::SteamGameServer;
using revive::legacy::SteamMasterServerUpdater;
using revive::legacy::SteamUtils;
using revive::legacy::kPipe;
using revive::legacy::kUser;

REVIVE_EXPORT void *CreateInterface(const char *name, int *returnCode)
{
    void *result = NULL;

    if (name)
    {
        if (std::strcmp(name, "SteamClient006") == 0)
            result = &SteamClient();
        else if (std::strcmp(name, "SteamGameServer002") == 0)
            result = &SteamGameServer();
        else if (std::strcmp(name, "SteamUtils001") == 0)
            result = &SteamUtils();
        else if (std::strcmp(name, "SteamMasterServerUpdater001") == 0)
            result = &SteamMasterServerUpdater();
    }

    if (returnCode)
        *returnCode = result ? 0 : 1;

    Log("Interface", "CreateInterface name=%s result=%p", name ? name : "(null)", result);
    return result;
}

REVIVE_EXPORT bool Steam_BGetCallback(HSteamPipe pipe, CallbackMsg_t *msg)
{
    return GetNextCallback(pipe, msg);
}

REVIVE_EXPORT void Steam_FreeLastCallback(HSteamPipe pipe)
{
    FreeLastCallback(pipe);
}

REVIVE_EXPORT bool Steam_GetAPICallResult(HSteamPipe, uint64, void *, int, int, bool *failed)
{
    if (failed) *failed = true;
    return false;
}

REVIVE_EXPORT HSteamUser Steam_GetHSteamUserCurrent()
{
    revive::legacy::RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    return state.currentCallbackUser ? state.currentCallbackUser : kUser;
}

// Build 4100 consumes callbacks through Steam_BGetCallback. Keep this ABI slot
// as a deliberate no-op without the old per-frame debug log spam.
REVIVE_EXPORT void Steam_RunCallbacks(HSteamPipe, bool) {}
REVIVE_EXPORT void Steam_RegisterInterfaceFuncs(void *) {}
REVIVE_EXPORT bool Steam_BConnected(HSteamUser, HSteamPipe) { return true; }
REVIVE_EXPORT bool Steam_BLoggedOn(HSteamUser, HSteamPipe) { return SteamGameServer().BLoggedOn(); }
REVIVE_EXPORT HSteamPipe Steam_CreateSteamPipe() { return kPipe; }
REVIVE_EXPORT bool Steam_BReleaseSteamPipe(HSteamPipe) { return true; }
REVIVE_EXPORT HSteamUser Steam_CreateGlobalUser(HSteamPipe *pipe) { return SteamClient().CreateGlobalUser(pipe); }
REVIVE_EXPORT HSteamUser Steam_ConnectToGlobalUser(HSteamPipe pipe) { return SteamClient().ConnectToGlobalUser(pipe); }
REVIVE_EXPORT HSteamUser Steam_CreateLocalUser(HSteamPipe *pipe, EAccountType) { return SteamClient().CreateLocalUser(pipe); }
REVIVE_EXPORT void Steam_ReleaseUser(HSteamPipe pipe, HSteamUser user) { SteamClient().ReleaseUser(pipe, user); }
REVIVE_EXPORT void Steam_SetLocalIPBinding(uint32 ip, uint16 port) { SteamClient().SetLocalIPBinding(ip, port); }
REVIVE_EXPORT int Steam_GSGetSteamGameConnectToken(HSteamUser, HSteamPipe, void *, int) { return 0; }

REVIVE_EXPORT void *Steam_GetGSHandle(HSteamUser user, HSteamPipe pipe)
{
    return SteamClient().GetISteamGameServer(user, pipe, "SteamGameServer002");
}

REVIVE_EXPORT void Steam_LogOn(HSteamUser, HSteamPipe, uint64)
{
    revive::legacy::RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.loggedOn = true;
}

REVIVE_EXPORT void Steam_LogOff(HSteamUser, HSteamPipe)
{
    revive::legacy::RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.loggedOn = false;
}

REVIVE_EXPORT bool Steam_GSBLoggedOn(void *handle)
{
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->BLoggedOn() : false;
}

REVIVE_EXPORT bool Steam_GSBSecure(void *handle)
{
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->BSecure() : false;
}

REVIVE_EXPORT void Steam_GSLogOn(void *handle)
{
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    if (server) server->LogOn();
}

REVIVE_EXPORT void Steam_GSLogOff(void *handle)
{
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    if (server) server->LogOff();
}

REVIVE_EXPORT void Steam_GSSetSpawnCount(void *handle, uint32 count)
{
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    if (server) server->GSSetSpawnCount(count);
}

REVIVE_EXPORT bool Steam_GSGetSteam2GetEncryptionKeyToSendToNewClient(void *handle, void *key,
                                                                      uint32 *size, uint32 maxSize)
{
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GSGetSteam2GetEncryptionKeyToSendToNewClient(key, size, maxSize) : false;
}

REVIVE_EXPORT bool Steam_GSSendSteam2UserConnect(void *handle, uint32 accountId, const void *rawKey,
                                                 uint32 rawKeyLen, uint32 ip, uint16 port,
                                                 const void *cookie, uint32 cookieLen)
{
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GSSendSteam2UserConnect(accountId, rawKey, rawKeyLen, ip, port, cookie, cookieLen) : false;
}

REVIVE_EXPORT bool Steam_GSSendSteam3UserConnect(void *handle, uint64 steamID, uint32 ip,
                                                 const void *cookie, uint32 cookieLen)
{
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GSSendSteam3UserConnect(CSteamID(steamID), ip, cookie, cookieLen) : false;
}

REVIVE_EXPORT bool Steam_GSRemoveUserConnect(void *handle, uint32 accountId)
{
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GSRemoveUserConnect(accountId) : false;
}

REVIVE_EXPORT bool Steam_GSSendUserDisconnect(void *handle, uint64 steamID, uint32 accountId)
{
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GSSendUserDisconnect(CSteamID(steamID), accountId) : false;
}

REVIVE_EXPORT bool Steam_GSSendUserStatusResponse(void *handle, uint64 steamID,
                                                  int secondsConnected, int secondsSinceLast)
{
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GSSendUserStatusResponse(CSteamID(steamID), secondsConnected, secondsSinceLast) : false;
}

REVIVE_EXPORT uint64 Steam_GSGetSteamID(void *handle)
{
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GetSteamID().ConvertToUint64() : 0;
}

REVIVE_EXPORT bool Steam_GSSetStatus(void *handle, int32 appIdServed, uint32 serverFlags,
                                     int players, int maxPlayers, int botPlayers, int gamePort,
                                     const char *serverName, const char *gameDir,
                                     const char *mapName, const char *version)
{
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->Obsolete_GSSetStatus(appIdServed, serverFlags, players, maxPlayers, botPlayers,
                                                 gamePort, serverName, gameDir, mapName, version) : false;
}

REVIVE_EXPORT bool Steam_GSSetServerType(void *handle, int32 appIdServed, uint32 serverFlags,
                                         uint32 gameIP, uint32 gamePort,
                                         const char *gameDir, const char *version)
{
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GSSetServerType(appIdServed, serverFlags, gameIP, gamePort, gameDir, version) : false;
}

REVIVE_EXPORT bool Steam_GSUpdateStatus(void *handle, int players, int maxPlayers, int bots,
                                       const char *name, const char *map)
{
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GSUpdateStatus(players, maxPlayers, bots, name, map) : false;
}

REVIVE_EXPORT int Steam_InitiateGameConnection(HSteamUser user, HSteamPipe pipe, void *blob, int maxBlob,
                                                uint64 steamID, int gameAppID, uint32 serverIP,
                                                uint16 serverPort, bool secure)
{
    Log("FlatAPI", "Steam_InitiateGameConnection user=%d pipe=%d blob=%p max=%d steamid64=%llu app=%d ip=0x%08x port=%u secure=%d unsupported=1",
        user, pipe, blob, maxBlob, static_cast<unsigned long long>(steamID), gameAppID, serverIP,
        static_cast<unsigned>(serverPort), secure ? 1 : 0);
    return 0;
}

REVIVE_EXPORT void Steam_TerminateGameConnection(HSteamUser user, HSteamPipe pipe,
                                                  uint32 serverIP, uint16 serverPort)
{
    Log("FlatAPI", "Steam_TerminateGameConnection user=%d pipe=%d ip=0x%08x port=%u",
        user, pipe, serverIP, static_cast<unsigned>(serverPort));
}

REVIVE_EXPORT const char *REVive_LegacySteamClient_BuildMarker()
{
    return "REVive legacy SteamClient006 backend M3.4-dev.1";
}
