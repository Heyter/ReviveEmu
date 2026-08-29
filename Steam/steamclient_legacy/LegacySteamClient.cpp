#include <cstring>

#include "LegacySteamCallbacks.h"
#include "LegacySteamInterfaces.h"
#include "LegacySteamRuntime.h"
#include "LegacySteamTrace.h"

#if defined(__GNUC__)
#define REVIVE_EXPORT extern "C" __attribute__((visibility("default")))
#else
#define REVIVE_EXPORT extern "C"
#endif

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
using revive::legacy::TraceAbiCall;
using revive::legacy::TraceAbiInterfaceQuery;
using revive::legacy::kAbiCompatibleNoop;
using revive::legacy::kAbiImplemented;
using revive::legacy::kAbiPartial;
using revive::legacy::kAbiUnsupported;
using revive::legacy::kPipe;
using revive::legacy::kUser;

REVIVE_EXPORT void *CreateInterface(const char *name, int *returnCode)
{
    TraceAbiCall("factory", "CreateInterface", kAbiImplemented);

    void *result = NULL;

    if (name)
    {
        if (std::strcmp(name, "SteamClient006") == 0)
            result = &SteamClient();
        else if (std::strcmp(name, "SteamGameServer002") == 0)
            result = &SteamGameServer();
        else if (std::strcmp(name, "SteamUtils001") == 0)
            result = &revive::legacy::SteamUtils();
        else if (std::strcmp(name, "SteamMasterServerUpdater001") == 0)
            result = &revive::legacy::SteamMasterServerUpdater();
    }

    if (returnCode)
        *returnCode = result ? 0 : 1;

    TraceAbiInterfaceQuery("CreateInterface", name, result != NULL);
    Log("Interface", "CreateInterface name=%s result=%p", name ? name : "(null)", result);
    return result;
}

REVIVE_EXPORT bool Steam_BGetCallback(HSteamPipe pipe, CallbackMsg_t *msg)
{
    TraceAbiCall("flat", "Steam_BGetCallback", kAbiImplemented);
    return GetNextCallback(pipe, msg);
}

REVIVE_EXPORT void Steam_FreeLastCallback(HSteamPipe pipe)
{
    TraceAbiCall("flat", "Steam_FreeLastCallback", kAbiImplemented);
    FreeLastCallback(pipe);
}

REVIVE_EXPORT bool Steam_GetAPICallResult(HSteamPipe, uint64, void *, int, int, bool *failed)
{
    TraceAbiCall("flat", "Steam_GetAPICallResult", kAbiUnsupported);
    if (failed) *failed = true;
    return false;
}

REVIVE_EXPORT HSteamUser Steam_GetHSteamUserCurrent()
{
    TraceAbiCall("flat", "Steam_GetHSteamUserCurrent", kAbiImplemented);
    revive::legacy::RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    return state.currentCallbackUser ? state.currentCallbackUser : kUser;
}

// Build 4100 consumes callbacks through Steam_BGetCallback. Keep this ABI slot
// as a deliberate no-op without the old per-frame debug log spam.
REVIVE_EXPORT void Steam_RunCallbacks(HSteamPipe, bool)
{
    TraceAbiCall("flat", "Steam_RunCallbacks", kAbiCompatibleNoop);
}

REVIVE_EXPORT void Steam_RegisterInterfaceFuncs(void *)
{
    TraceAbiCall("flat", "Steam_RegisterInterfaceFuncs", kAbiCompatibleNoop);
}

REVIVE_EXPORT bool Steam_BConnected(HSteamUser, HSteamPipe)
{
    TraceAbiCall("flat", "Steam_BConnected", kAbiImplemented);
    return true;
}

REVIVE_EXPORT bool Steam_BLoggedOn(HSteamUser, HSteamPipe)
{
    TraceAbiCall("flat", "Steam_BLoggedOn", kAbiPartial);
    return SteamGameServer().BLoggedOn();
}

REVIVE_EXPORT HSteamPipe Steam_CreateSteamPipe()
{
    TraceAbiCall("flat", "Steam_CreateSteamPipe", kAbiImplemented);
    return kPipe;
}

REVIVE_EXPORT bool Steam_BReleaseSteamPipe(HSteamPipe)
{
    TraceAbiCall("flat", "Steam_BReleaseSteamPipe", kAbiImplemented);
    return true;
}

REVIVE_EXPORT HSteamUser Steam_CreateGlobalUser(HSteamPipe *pipe)
{
    TraceAbiCall("flat", "Steam_CreateGlobalUser", kAbiImplemented);
    return SteamClient().CreateGlobalUser(pipe);
}

REVIVE_EXPORT HSteamUser Steam_ConnectToGlobalUser(HSteamPipe pipe)
{
    TraceAbiCall("flat", "Steam_ConnectToGlobalUser", kAbiImplemented);
    return SteamClient().ConnectToGlobalUser(pipe);
}

REVIVE_EXPORT HSteamUser Steam_CreateLocalUser(HSteamPipe *pipe, EAccountType)
{
    TraceAbiCall("flat", "Steam_CreateLocalUser", kAbiImplemented);
    return SteamClient().CreateLocalUser(pipe);
}

REVIVE_EXPORT void Steam_ReleaseUser(HSteamPipe pipe, HSteamUser user)
{
    TraceAbiCall("flat", "Steam_ReleaseUser", kAbiCompatibleNoop);
    SteamClient().ReleaseUser(pipe, user);
}

REVIVE_EXPORT void Steam_SetLocalIPBinding(uint32 ip, uint16 port)
{
    TraceAbiCall("flat", "Steam_SetLocalIPBinding", kAbiImplemented);
    SteamClient().SetLocalIPBinding(ip, port);
}

REVIVE_EXPORT int Steam_GSGetSteamGameConnectToken(HSteamUser, HSteamPipe, void *, int)
{
    TraceAbiCall("flat", "Steam_GSGetSteamGameConnectToken", kAbiUnsupported);
    return 0;
}

REVIVE_EXPORT void *Steam_GetGSHandle(HSteamUser user, HSteamPipe pipe)
{
    TraceAbiCall("flat", "Steam_GetGSHandle", kAbiImplemented);
    return SteamClient().GetISteamGameServer(user, pipe, "SteamGameServer002");
}

REVIVE_EXPORT void Steam_LogOn(HSteamUser, HSteamPipe, uint64)
{
    TraceAbiCall("flat", "Steam_LogOn", kAbiPartial);
    revive::legacy::RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.loggedOn = true;
}

REVIVE_EXPORT void Steam_LogOff(HSteamUser, HSteamPipe)
{
    TraceAbiCall("flat", "Steam_LogOff", kAbiPartial);
    revive::legacy::RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.loggedOn = false;
}

REVIVE_EXPORT bool Steam_GSBLoggedOn(void *handle)
{
    TraceAbiCall("flat", "Steam_GSBLoggedOn", kAbiImplemented);
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->BLoggedOn() : false;
}

REVIVE_EXPORT bool Steam_GSBSecure(void *handle)
{
    TraceAbiCall("flat", "Steam_GSBSecure", kAbiImplemented);
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->BSecure() : false;
}

REVIVE_EXPORT void Steam_GSLogOn(void *handle)
{
    TraceAbiCall("flat", "Steam_GSLogOn", kAbiImplemented);
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    if (server) server->LogOn();
}

REVIVE_EXPORT void Steam_GSLogOff(void *handle)
{
    TraceAbiCall("flat", "Steam_GSLogOff", kAbiImplemented);
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    if (server) server->LogOff();
}

REVIVE_EXPORT void Steam_GSSetSpawnCount(void *handle, uint32 count)
{
    TraceAbiCall("flat", "Steam_GSSetSpawnCount", kAbiCompatibleNoop);
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    if (server) server->GSSetSpawnCount(count);
}

REVIVE_EXPORT bool Steam_GSGetSteam2GetEncryptionKeyToSendToNewClient(void *handle, void *key,
                                                                      uint32 *size, uint32 maxSize)
{
    TraceAbiCall("flat", "Steam_GSGetSteam2GetEncryptionKeyToSendToNewClient", kAbiImplemented);
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GSGetSteam2GetEncryptionKeyToSendToNewClient(key, size, maxSize) : false;
}

REVIVE_EXPORT bool Steam_GSSendSteam2UserConnect(void *handle, uint32 accountId, const void *rawKey,
                                                 uint32 rawKeyLen, uint32 ip, uint16 port,
                                                 const void *cookie, uint32 cookieLen)
{
    TraceAbiCall("flat", "Steam_GSSendSteam2UserConnect", kAbiImplemented);
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GSSendSteam2UserConnect(accountId, rawKey, rawKeyLen, ip, port, cookie, cookieLen) : false;
}

REVIVE_EXPORT bool Steam_GSSendSteam3UserConnect(void *handle, uint64 steamID, uint32 ip,
                                                 const void *cookie, uint32 cookieLen)
{
    TraceAbiCall("flat", "Steam_GSSendSteam3UserConnect", kAbiPartial);
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GSSendSteam3UserConnect(CSteamID(steamID), ip, cookie, cookieLen) : false;
}

REVIVE_EXPORT bool Steam_GSRemoveUserConnect(void *handle, uint32 accountId)
{
    TraceAbiCall("flat", "Steam_GSRemoveUserConnect", kAbiImplemented);
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GSRemoveUserConnect(accountId) : false;
}

REVIVE_EXPORT bool Steam_GSSendUserDisconnect(void *handle, uint64 steamID, uint32 accountId)
{
    TraceAbiCall("flat", "Steam_GSSendUserDisconnect", kAbiImplemented);
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GSSendUserDisconnect(CSteamID(steamID), accountId) : false;
}

REVIVE_EXPORT bool Steam_GSSendUserStatusResponse(void *handle, uint64 steamID,
                                                  int secondsConnected, int secondsSinceLast)
{
    TraceAbiCall("flat", "Steam_GSSendUserStatusResponse", kAbiCompatibleNoop);
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GSSendUserStatusResponse(CSteamID(steamID), secondsConnected, secondsSinceLast) : false;
}

REVIVE_EXPORT uint64 Steam_GSGetSteamID(void *handle)
{
    TraceAbiCall("flat", "Steam_GSGetSteamID", kAbiImplemented);
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GetSteamID().ConvertToUint64() : 0;
}

REVIVE_EXPORT bool Steam_GSSetStatus(void *handle, int32 appIdServed, uint32 serverFlags,
                                     int players, int maxPlayers, int botPlayers, int gamePort,
                                     const char *serverName, const char *gameDir,
                                     const char *mapName, const char *version)
{
    TraceAbiCall("flat", "Steam_GSSetStatus", kAbiPartial);
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->Obsolete_GSSetStatus(appIdServed, serverFlags, players, maxPlayers, botPlayers,
                                                 gamePort, serverName, gameDir, mapName, version) : false;
}

REVIVE_EXPORT bool Steam_GSSetServerType(void *handle, int32 appIdServed, uint32 serverFlags,
                                         uint32 gameIP, uint32 gamePort,
                                         const char *gameDir, const char *version)
{
    TraceAbiCall("flat", "Steam_GSSetServerType", kAbiImplemented);
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GSSetServerType(appIdServed, serverFlags, gameIP, gamePort, gameDir, version) : false;
}

REVIVE_EXPORT bool Steam_GSUpdateStatus(void *handle, int players, int maxPlayers, int bots,
                                       const char *name, const char *map)
{
    TraceAbiCall("flat", "Steam_GSUpdateStatus", kAbiCompatibleNoop);
    CSteamGameServer002 *server = GameServerFromHandle(handle);
    return server ? server->GSUpdateStatus(players, maxPlayers, bots, name, map) : false;
}

REVIVE_EXPORT int Steam_InitiateGameConnection(HSteamUser user, HSteamPipe pipe, void *blob, int maxBlob,
                                                uint64 steamID, int gameAppID, uint32 serverIP,
                                                uint16 serverPort, bool secure)
{
    TraceAbiCall("flat", "Steam_InitiateGameConnection", kAbiUnsupported);
    Log("FlatAPI", "Steam_InitiateGameConnection user=%d pipe=%d blob=%p max=%d steamid64=%llu app=%d ip=0x%08x port=%u secure=%d unsupported=1",
        user, pipe, blob, maxBlob, static_cast<unsigned long long>(steamID), gameAppID, serverIP,
        static_cast<unsigned>(serverPort), secure ? 1 : 0);
    return 0;
}

REVIVE_EXPORT void Steam_TerminateGameConnection(HSteamUser user, HSteamPipe pipe,
                                                  uint32 serverIP, uint16 serverPort)
{
    TraceAbiCall("flat", "Steam_TerminateGameConnection", kAbiCompatibleNoop);
    Log("FlatAPI", "Steam_TerminateGameConnection user=%d pipe=%d ip=0x%08x port=%u",
        user, pipe, serverIP, static_cast<unsigned>(serverPort));
}

REVIVE_EXPORT const char *REVive_LegacySteamClient_BuildMarker()
{
    return "REVive legacy SteamClient006 backend M3.8-dev.1";
}
