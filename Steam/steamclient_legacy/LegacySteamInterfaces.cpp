#include "LegacySteamInterfaces.h"

#include <cstring>
#include <ctime>

#include "../steam/ClassicRevEmuTicket.h"
#include "LegacySteamAuth.h"
#include "LegacySteamCallbacks.h"
#include "LegacySteamTrace.h"

namespace revive
{
namespace legacy
{

namespace
{
CSteamGameServer002 g_gameServer;
CSteamUtils001 g_utils;
CSteamMasterServerUpdater001 g_masterUpdater;
CSteamClient006 g_client;

void CopyText(char *dst, size_t dstSize, const char *src)
{
    if (!dst || dstSize == 0)
        return;
    const char *safe = src ? src : "";
    std::strncpy(dst, safe, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

int FindMasterServer(const MasterServerState &master, const char *serverAddress)
{
    if (!serverAddress)
        return -1;
    for (size_t i = 0; i < master.masterServerCount; ++i)
    {
        if (std::strcmp(master.masterServers[i], serverAddress) == 0)
            return static_cast<int>(i);
    }
    return -1;
}
} // namespace

CSteamClient006 &SteamClient() { return g_client; }
CSteamGameServer002 &SteamGameServer() { return g_gameServer; }
CSteamUtils001 &SteamUtils() { return g_utils; }
CSteamMasterServerUpdater001 &SteamMasterServerUpdater() { return g_masterUpdater; }

CSteamGameServer002 *GameServerFromHandle(void *handle)
{
    return handle == &g_gameServer ? &g_gameServer : NULL;
}

HSteamPipe CSteamClient006::CreateSteamPipe() { TraceAbiCall("SteamClient006", "CreateSteamPipe", kAbiImplemented); return kPipe; }
bool CSteamClient006::BReleaseSteamPipe(HSteamPipe) { TraceAbiCall("SteamClient006", "BReleaseSteamPipe", kAbiImplemented); return true; }
HSteamUser CSteamClient006::CreateGlobalUser(HSteamPipe *pipe) { TraceAbiCall("SteamClient006", "CreateGlobalUser", kAbiImplemented); if (pipe) *pipe = kPipe; return kUser; }
HSteamUser CSteamClient006::ConnectToGlobalUser(HSteamPipe) { TraceAbiCall("SteamClient006", "ConnectToGlobalUser", kAbiImplemented); return kUser; }
HSteamUser CSteamClient006::CreateLocalUser(HSteamPipe *pipe)
{
    TraceAbiCall("SteamClient006", "CreateLocalUser", kAbiImplemented);
    if (pipe) *pipe = kPipe;
    Log("Client", "CreateLocalUser pipe=%d user=%d", kPipe, kUser);
    return kUser;
}
void CSteamClient006::ReleaseUser(HSteamPipe, HSteamUser) { TraceAbiCall("SteamClient006", "ReleaseUser", kAbiCompatibleNoop); }
void *CSteamClient006::GetISteamUser(HSteamUser, HSteamPipe, const char *version) { TraceAbiCall("SteamClient006", "GetISteamUser", kAbiUnsupported); TraceAbiInterfaceQuery("SteamClient006.GetISteamUser", version, false); return NULL; }
void *CSteamClient006::GetIVAC(HSteamUser) { TraceAbiCall("SteamClient006", "GetIVAC", kAbiUnsupported); return NULL; }

void *CSteamClient006::GetISteamGameServer(HSteamUser, HSteamPipe, const char *version)
{
    TraceAbiCall("SteamClient006", "GetISteamGameServer", kAbiImplemented);
    Log("Client", "GetISteamGameServer version=%s", version ? version : "(null)");
    void *result = version && std::strcmp(version, "SteamGameServer002") == 0 ? &g_gameServer : NULL;
    TraceAbiInterfaceQuery("SteamClient006.GetISteamGameServer", version, result != NULL);
    return result;
}

void CSteamClient006::SetLocalIPBinding(uint32 ip, uint16 port)
{
    TraceAbiCall("SteamClient006", "SetLocalIPBinding", kAbiImplemented);
    RuntimeState &state = Runtime();
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.localIP = ip;
        state.localPort = port;
    }
    Log("Client", "SetLocalIPBinding ip=0x%08x port=%u", ip, static_cast<unsigned>(port));
}

const char *CSteamClient006::GetUniverseName(EUniverse universe)
{
    TraceAbiCall("SteamClient006", "GetUniverseName", kAbiImplemented);
    switch (universe)
    {
        case k_EUniversePublic: return "Public";
        case k_EUniverseBeta: return "Beta";
        case k_EUniverseInternal: return "Internal";
        case k_EUniverseDev: return "Dev";
        default: return "Invalid";
    }
}

void *CSteamClient006::GetISteamFriends(HSteamUser, HSteamPipe, const char *version) { TraceAbiCall("SteamClient006", "GetISteamFriends", kAbiUnsupported); TraceAbiInterfaceQuery("SteamClient006.GetISteamFriends", version, false); return NULL; }

void *CSteamClient006::GetISteamUtils(HSteamPipe, const char *version)
{
    TraceAbiCall("SteamClient006", "GetISteamUtils", kAbiImplemented);
    Log("Client", "GetISteamUtils version=%s", version ? version : "(null)");
    void *result = version && std::strcmp(version, "SteamUtils001") == 0 ? &g_utils : NULL;
    TraceAbiInterfaceQuery("SteamClient006.GetISteamUtils", version, result != NULL);
    return result;
}

void *CSteamClient006::GetISteamBilling(HSteamUser, HSteamPipe, const char *version) { TraceAbiCall("SteamClient006", "GetISteamBilling", kAbiUnsupported); TraceAbiInterfaceQuery("SteamClient006.GetISteamBilling", version, false); return NULL; }
void *CSteamClient006::GetISteamMatchMaking(HSteamUser, HSteamPipe, const char *version) { TraceAbiCall("SteamClient006", "GetISteamMatchmaking", kAbiUnsupported); TraceAbiInterfaceQuery("SteamClient006.GetISteamMatchmaking", version, false); return NULL; }
void *CSteamClient006::GetISteamContentServer(HSteamUser, HSteamPipe, const char *version) { TraceAbiCall("SteamClient006", "GetISteamContentServer", kAbiUnsupported); TraceAbiInterfaceQuery("SteamClient006.GetISteamContentServer", version, false); return NULL; }
void *CSteamClient006::GetISteamApps(HSteamUser, HSteamPipe, const char *version) { TraceAbiCall("SteamClient006", "GetISteamApps", kAbiUnsupported); TraceAbiInterfaceQuery("SteamClient006.GetISteamApps", version, false); return NULL; }

void *CSteamClient006::GetISteamMasterServerUpdater(HSteamUser, HSteamPipe, const char *version)
{
    TraceAbiCall("SteamClient006", "GetISteamMasterServerUpdater", kAbiImplemented);
    Log("Client", "GetISteamMasterServerUpdater version=%s", version ? version : "(null)");
    void *result = version && std::strcmp(version, "SteamMasterServerUpdater001") == 0 ? &g_masterUpdater : NULL;
    TraceAbiInterfaceQuery("SteamClient006.GetISteamMasterServerUpdater", version, result != NULL);
    return result;
}

void *CSteamClient006::GetISteamMatchMakingServers(HSteamUser, HSteamPipe, const char *version) { TraceAbiCall("SteamClient006", "GetISteamMatchmakingServers", kAbiUnsupported); TraceAbiInterfaceQuery("SteamClient006.GetISteamMatchmakingServers", version, false); return NULL; }
void CSteamClient006::RunFrame() { TraceAbiCall("SteamClient006", "RunFrame", kAbiCompatibleNoop); }
uint32 CSteamClient006::GetIPCCallCount() { TraceAbiCall("SteamClient006", "GetIPCCallCount", kAbiCompatibleNoop); return 0; }

void CSteamGameServer002::LogOn()
{
    TraceAbiCall("SteamGameServer002", "LogOn", kAbiImplemented);
    RuntimeState &state = Runtime();
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        if (!state.loggedOn)
        {
            state.loggedOn = true;
            QueueCallbackLocked(state, kCallbackSteamServersConnected, NULL, 0, 0, 0, false);
            changed = true;
        }
    }
    if (changed)
        Log("GameServer", "LogOn");
}

void CSteamGameServer002::LogOff()
{
    TraceAbiCall("SteamGameServer002", "LogOff", kAbiImplemented);
    RuntimeState &state = Runtime();
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.loggedOn = false;
    }
    Log("GameServer", "LogOff");
}

bool CSteamGameServer002::BLoggedOn()
{
    TraceAbiCall("SteamGameServer002", "BLoggedOn", kAbiImplemented);
    RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    return state.loggedOn;
}

void CSteamGameServer002::GSSetSpawnCount(uint32 count)
{
    TraceAbiCall("SteamGameServer002", "SetSpawnCount", kAbiCompatibleNoop);
    Log("GameServer", "GSSetSpawnCount count=%u", count);
}

bool CSteamGameServer002::GSGetSteam2GetEncryptionKeyToSendToNewClient(void *key, uint32 *keySize, uint32 maxKeySize)
{
    TraceAbiCall("SteamGameServer002", "GetSteam2GetEncryptionKeyToSendToNewClient", kAbiImplemented);
    if (!key || !keySize || maxKeySize < sizeof(g_TicketKey))
    {
        if (keySize) *keySize = 0;
        Log("Auth", "GSGetSteam2GetEncryptionKeyToSendToNewClient rejected max=%u", maxKeySize);
        return false;
    }

    std::memcpy(key, g_TicketKey, sizeof(g_TicketKey));
    *keySize = static_cast<uint32>(sizeof(g_TicketKey));
    Log("Auth", "GSGetSteam2GetEncryptionKeyToSendToNewClient key_size=%u max=%u", *keySize, maxKeySize);
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
    TraceAbiCall("SteamGameServer002", "SendSteam2UserConnect", kAbiImplemented);
    ClassicRevEmuTicketInfo info;
    const ClassicRevEmuTicketResult ticketResult = ValidateClassicRevEmuTicket(rawKey, rawKeyLen, &info);

    if (ticketResult != kClassicRevEmuTicketValid)
    {
        Log("Auth", "Steam2 auth REJECT account=%u ip=0x%08x port=%u raw_len=%u cookie_len=%u reason=%s",
            accountId, ip, static_cast<unsigned>(port), rawKeyLen, cookieLen,
            ClassicRevEmuTicketResultString(ticketResult));
        return false;
    }

    const CSteamID steamID = info.steamID;
    const uint64 steamID64 = steamID.ConvertToUint64();
    Steam2RegistrationResult registration;

    RuntimeState &state = Runtime();
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        registration = RegisterSteam2UserLocked(state, accountId, steamID);
        if (registration == kSteam2RegistrationAccepted)
        {
            // The identity insert and 205 -> 201 pair are one transaction.
            // Whole client pairs may interleave only between transactions.
            QueueSteam2AuthCallbacksLocked(state, accountId, steamID);
        }
    }

    char steam2[64];
    if (registration == kSteam2RegistrationAccountConflict ||
        registration == kSteam2RegistrationDuplicateSteamID)
    {
        Log("Auth", "Steam2 auth REJECT account=%u ip=0x%08x port=%u raw_len=%u cookie_len=%u reason=%s steam2=%s steamid64=%llu",
            accountId, ip, static_cast<unsigned>(port), rawKeyLen, cookieLen,
            Steam2RegistrationResultString(registration),
            Steam2String(steamID, steam2, sizeof(steam2)),
            static_cast<unsigned long long>(steamID64));
        return false;
    }

    if (registration == kSteam2RegistrationIdempotent)
    {
        Log("Auth", "Steam2 auth IDEMPOTENT account=%u steam2=%s steamid64=%llu callbacks=0",
            accountId, Steam2String(steamID, steam2, sizeof(steam2)),
            static_cast<unsigned long long>(steamID64));
        return true;
    }

    Log("Auth", "GSSendSteam2UserConnect account=%u ip=0x%08x port=%u raw_len=%u cookie_len=%u type=ClassicRevEmu steam2=%s steamid64=%llu",
        accountId, ip, static_cast<unsigned>(port), rawKeyLen, cookieLen,
        Steam2String(steamID, steam2, sizeof(steam2)),
        static_cast<unsigned long long>(steamID64));
    Log("Auth", "ClassicRevEmu hash=%u steamid_low=0x%08x steamid_high=0x%08x",
        info.hash, info.steamIDLow, info.steamIDHigh);
    return true;
}

bool CSteamGameServer002::GSSendSteam3UserConnect(CSteamID steamID, uint32 ip, const void *, uint32)
{
    TraceAbiCall("SteamGameServer002", "SendSteam3UserConnect", kAbiPartial);
    char steam2[64];
    Log("Auth", "GSSendSteam3UserConnect ip=0x%08x steam2=%s", ip,
        Steam2String(steamID, steam2, sizeof(steam2)));
    QueueClientApprove(steamID);
    return true;
}

bool CSteamGameServer002::GSRemoveUserConnect(uint32 accountId)
{
    TraceAbiCall("SteamGameServer002", "RemoveUserConnect", kAbiImplemented);
    RuntimeState &state = Runtime();
    size_t erased;
    size_t callbacksRemoved;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        erased = RemoveSteam2UserLocked(state, accountId);
        callbacksRemoved = RemovePendingAuthCallbacksLocked(state, accountId);
    }
    Log("Auth", "GSRemoveUserConnect account=%u state_removed=%u callbacks_removed=%u", accountId,
        static_cast<unsigned>(erased), static_cast<unsigned>(callbacksRemoved));
    return true;
}

bool CSteamGameServer002::GSSendUserDisconnect(CSteamID steamID, uint32 accountId)
{
    TraceAbiCall("SteamGameServer002", "SendUserDisconnect", kAbiImplemented);
    const uint64 steamID64 = steamID.ConvertToUint64();
    RuntimeState &state = Runtime();
    uint64 expectedSteamID = 0;
    size_t callbacksRemoved = 0;
    Steam2DisconnectResult result;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        result = DisconnectSteam2UserLocked(state, accountId, steamID, &expectedSteamID);
        if (result == kSteam2DisconnectRemoved)
            callbacksRemoved = RemovePendingAuthCallbacksLocked(state, accountId);
    }

    if (result == kSteam2DisconnectAlreadyAbsent)
    {
        Log("Auth", "GSSendUserDisconnect account=%u steamid64=%llu state=already_absent", accountId,
            static_cast<unsigned long long>(steamID64));
        return true;
    }

    if (result == kSteam2DisconnectIdentityMismatch)
    {
        Log("Auth", "GSSendUserDisconnect REJECT account=%u steamid64=%llu expected=%llu reason=identity_mismatch",
            accountId, static_cast<unsigned long long>(steamID64),
            static_cast<unsigned long long>(expectedSteamID));
        return false;
    }

    Log("Auth", "GSSendUserDisconnect account=%u steamid64=%llu callbacks_removed=%u", accountId,
        static_cast<unsigned long long>(steamID64), static_cast<unsigned>(callbacksRemoved));
    return true;
}

bool CSteamGameServer002::GSSendUserStatusResponse(CSteamID, int, int) { TraceAbiCall("SteamGameServer002", "SendUserStatusResponse", kAbiCompatibleNoop); return true; }

bool CSteamGameServer002::Obsolete_GSSetStatus(int32 appIdServed, uint32 serverFlags,
                                               int players, int maxPlayers, int botPlayers, int gamePort,
                                               const char *serverName, const char *gameDir,
                                               const char *mapName, const char *version)
{
    TraceAbiCall("SteamGameServer002", "Obsolete_GSSetStatus", kAbiPartial);
    Log("GameServer", "Obsolete_GSSetStatus app=%d flags=0x%08x players=%d/%d bots=%d port=%d name=%s dir=%s map=%s version=%s",
        appIdServed, serverFlags, players, maxPlayers, botPlayers, gamePort,
        serverName ? serverName : "", gameDir ? gameDir : "", mapName ? mapName : "", version ? version : "");
    return true;
}

bool CSteamGameServer002::GSUpdateStatus(int, int, int, const char *, const char *) { TraceAbiCall("SteamGameServer002", "UpdateStatus", kAbiCompatibleNoop); return true; }
bool CSteamGameServer002::BSecure() { TraceAbiCall("SteamGameServer002", "BSecure", kAbiImplemented); return false; }
CSteamID CSteamGameServer002::GetSteamID() { TraceAbiCall("SteamGameServer002", "GetSteamID", kAbiImplemented); return MakeServerID(); }

bool CSteamGameServer002::GSSetServerType(int32 gameAppId, uint32 serverFlags, uint32 gameIP,
                                          uint32 gamePort, const char *gameDir, const char *version)
{
    TraceAbiCall("SteamGameServer002", "SetServerType", kAbiImplemented);
    RuntimeState &state = Runtime();
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        if (gameIP) state.localIP = gameIP;
        if (gamePort) state.localPort = static_cast<uint16>(gamePort);
    }
    Log("GameServer", "GSSetServerType app=%d flags=0x%08x ip=0x%08x game=%u dir=%s version=%s",
        gameAppId, serverFlags, gameIP, gamePort,
        gameDir ? gameDir : "", version ? version : "");
    return true;
}

bool CSteamGameServer002::GSSetServerType2(int32 gameAppId, uint32 serverFlags, uint32 gameIP,
                                           uint16 gamePort, uint16 spectatorPort, uint16 queryPort,
                                           const char *gameDir, const char *version, bool lanMode)
{
    TraceAbiCall("SteamGameServer002", "SetServerType2", kAbiImplemented);
    RuntimeState &state = Runtime();
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        if (gameIP) state.localIP = gameIP;
        if (gamePort) state.localPort = gamePort;
    }
    Log("GameServer", "GSSetServerType2 app=%d flags=0x%08x ip=0x%08x game=%u spectator=%u query=%u dir=%s version=%s lan=%d",
        gameAppId, serverFlags, gameIP,
        static_cast<unsigned>(gamePort), static_cast<unsigned>(spectatorPort), static_cast<unsigned>(queryPort),
        gameDir ? gameDir : "", version ? version : "", lanMode ? 1 : 0);
    return true;
}

bool CSteamGameServer002::GSUpdateStatus2(int players, int maxPlayers, int botPlayers,
                                          const char *serverName, const char *spectatorServerName,
                                          const char *mapName)
{
    TraceAbiCall("SteamGameServer002", "UpdateStatus2", kAbiCompatibleNoop);
    Log("GameServer", "GSUpdateStatus2 players=%d/%d bots=%d name=%s spectator=%s map=%s",
        players, maxPlayers, botPlayers, serverName ? serverName : "",
        spectatorServerName ? spectatorServerName : "", mapName ? mapName : "");
    return true;
}

bool CSteamGameServer002::GSCreateUnauthenticatedUser(CSteamID *steamID)
{
    TraceAbiCall("SteamGameServer002", "CreateUnauthenticatedUser", kAbiImplemented);
    if (!steamID)
        return false;

    RuntimeState &state = Runtime();
    uint32 account;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        account = 0xF0000000u | (state.botCounter++ & 0x0fffffffu);
    }
    steamID->InstancedSet(account, 1, k_EUniversePublic, k_EAccountTypeAnonUser);
    return true;
}

bool CSteamGameServer002::GSSetUserData(CSteamID, const char *, uint32) { TraceAbiCall("SteamGameServer002", "SetUserData", kAbiCompatibleNoop); return true; }
void CSteamGameServer002::GSUpdateSpectatorPort(uint16) { TraceAbiCall("SteamGameServer002", "UpdateSpectatorPort", kAbiCompatibleNoop); }
void CSteamGameServer002::GSSetGameType(const char *) { TraceAbiCall("SteamGameServer002", "SetGameType", kAbiCompatibleNoop); }

uint32 CSteamUtils001::GetSecondsSinceAppActive() { TraceAbiCall("SteamUtils001", "GetSecondsSinceAppActive", kAbiCompatibleNoop); return 0; }
uint32 CSteamUtils001::GetSecondsSinceComputerActive() { TraceAbiCall("SteamUtils001", "GetSecondsSinceComputerActive", kAbiCompatibleNoop); return 0; }
EUniverse CSteamUtils001::GetConnectedUniverse() { TraceAbiCall("SteamUtils001", "GetConnectedUniverse", kAbiImplemented); return k_EUniversePublic; }
uint32 CSteamUtils001::GetServerRealTime() { TraceAbiCall("SteamUtils001", "GetServerRealTime", kAbiImplemented); return static_cast<uint32>(std::time(NULL)); }

void CSteamMasterServerUpdater001::SetActive(bool active)
{
    TraceAbiCall("SteamMasterServerUpdater001", "SetActive", kAbiImplemented);
    RuntimeState &state = Runtime();
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.masterServer.active = active;
        if (active)
            state.masterServer.shutdownNotified = false;
    }
    Log("MasterServer", "SetActive=%d", active ? 1 : 0);
}

void CSteamMasterServerUpdater001::SetHeartbeatInterval(int interval)
{
    TraceAbiCall("SteamMasterServerUpdater001", "SetHeartbeatInterval", kAbiImplemented);
    RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.masterServer.heartbeatInterval = interval;
}

bool CSteamMasterServerUpdater001::HandleIncomingPacket(const void *, int, uint32, uint16)
{
    // Packet-level master-server protocol is intentionally out of scope while
    // MASTER_SERVER=0. Keep this explicit so a future runtime call is visible.
    TraceAbiCall("SteamMasterServerUpdater001", "HandleIncomingPacket", kAbiUnsupported);
    return false;
}

int CSteamMasterServerUpdater001::GetNextOutgoingPacket(void *, int, uint32 *ip, uint16 *port)
{
    TraceAbiCall("SteamMasterServerUpdater001", "GetNextOutgoingPacket", kAbiUnsupported);
    if (ip) *ip = 0;
    if (port) *port = 0;
    return 0;
}

void CSteamMasterServerUpdater001::SetBasicServerData(uint16 protocolVersion, bool dedicated,
                                                       const char *region, const char *productName,
                                                       uint16 maxClients, bool passwordProtected,
                                                       const char *gameDescription)
{
    TraceAbiCall("SteamMasterServerUpdater001", "SetBasicServerData", kAbiImplemented);
    RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.masterServer.protocolVersion = protocolVersion;
    state.masterServer.dedicated = dedicated;
    CopyText(state.masterServer.regionName, sizeof(state.masterServer.regionName), region);
    CopyText(state.masterServer.productName, sizeof(state.masterServer.productName), productName);
    state.masterServer.maxClients = maxClients;
    state.masterServer.passwordProtected = passwordProtected;
    CopyText(state.masterServer.gameDescription, sizeof(state.masterServer.gameDescription), gameDescription);
}

void CSteamMasterServerUpdater001::ClearAllKeyValues()
{
    TraceAbiCall("SteamMasterServerUpdater001", "ClearAllKeyValues", kAbiImplemented);
    RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    std::memset(state.masterServer.keyValues, 0, sizeof(state.masterServer.keyValues));
}

void CSteamMasterServerUpdater001::SetKeyValue(const char *key, const char *value)
{
    TraceAbiCall("SteamMasterServerUpdater001", "SetKeyValue", kAbiImplemented);
    if (!key || !*key)
        return;

    RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    MasterServerState &master = state.masterServer;
    MasterKeyValue *freeSlot = NULL;
    for (size_t i = 0; i < sizeof(master.keyValues) / sizeof(master.keyValues[0]); ++i)
    {
        MasterKeyValue &entry = master.keyValues[i];
        if (entry.used && std::strcmp(entry.key, key) == 0)
        {
            CopyText(entry.value, sizeof(entry.value), value);
            return;
        }
        if (!entry.used && !freeSlot)
            freeSlot = &entry;
    }
    if (!freeSlot)
        return;
    freeSlot->used = true;
    CopyText(freeSlot->key, sizeof(freeSlot->key), key);
    CopyText(freeSlot->value, sizeof(freeSlot->value), value);
}

void CSteamMasterServerUpdater001::NotifyShutdown()
{
    TraceAbiCall("SteamMasterServerUpdater001", "NotifyShutdown", kAbiImplemented);
    RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.masterServer.active = false;
    state.masterServer.shutdownNotified = true;
}

bool CSteamMasterServerUpdater001::WasRestartRequested()
{
    // A restart request can only arrive through the currently unsupported
    // master-server packet protocol. Returning false is the safe compatibility
    // behavior when master communication is disabled.
    TraceAbiCall("SteamMasterServerUpdater001", "WasRestartRequested", kAbiCompatibleNoop);
    return false;
}

void CSteamMasterServerUpdater001::ForceHeartbeat()
{
    // Preserve the request in local state. Actual UDP emission remains outside
    // this compatibility layer until master-server networking becomes scope.
    TraceAbiCall("SteamMasterServerUpdater001", "ForceHeartbeat", kAbiPartial);
    RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.masterServer.heartbeatForced = true;
}

bool CSteamMasterServerUpdater001::AddMasterServer(const char *serverAddress)
{
    TraceAbiCall("SteamMasterServerUpdater001", "AddMasterServer", kAbiImplemented);
    if (!serverAddress || !*serverAddress)
        return false;

    RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    MasterServerState &master = state.masterServer;
    if (FindMasterServer(master, serverAddress) >= 0)
        return true;
    if (master.masterServerCount >= sizeof(master.masterServers) / sizeof(master.masterServers[0]))
        return false;
    CopyText(master.masterServers[master.masterServerCount], sizeof(master.masterServers[0]), serverAddress);
    ++master.masterServerCount;
    return true;
}

bool CSteamMasterServerUpdater001::RemoveMasterServer(const char *serverAddress)
{
    TraceAbiCall("SteamMasterServerUpdater001", "RemoveMasterServer", kAbiImplemented);
    if (!serverAddress || !*serverAddress)
        return false;

    RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    MasterServerState &master = state.masterServer;
    const int index = FindMasterServer(master, serverAddress);
    if (index < 0)
        return false;
    const size_t pos = static_cast<size_t>(index);
    for (size_t i = pos + 1; i < master.masterServerCount; ++i)
        std::memcpy(master.masterServers[i - 1], master.masterServers[i], sizeof(master.masterServers[i]));
    --master.masterServerCount;
    master.masterServers[master.masterServerCount][0] = '\0';
    return true;
}

int CSteamMasterServerUpdater001::GetNumMasterServers()
{
    TraceAbiCall("SteamMasterServerUpdater001", "GetNumMasterServers", kAbiImplemented);
    RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    return static_cast<int>(state.masterServer.masterServerCount);
}

int CSteamMasterServerUpdater001::GetMasterServerAddress(int serverIndex, char *address, int addressSize)
{
    TraceAbiCall("SteamMasterServerUpdater001", "GetMasterServerAddress", kAbiImplemented);
    if (!address || addressSize <= 0)
        return 0;
    address[0] = '\0';

    RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    MasterServerState &master = state.masterServer;
    if (serverIndex < 0 || static_cast<size_t>(serverIndex) >= master.masterServerCount)
        return 0;

    const char *server = master.masterServers[static_cast<size_t>(serverIndex)];
    const size_t sourceLength = std::strlen(server);
    const size_t maxCopy = static_cast<size_t>(addressSize - 1);
    const size_t bytes = sourceLength < maxCopy ? sourceLength : maxCopy;
    std::memcpy(address, server, bytes);
    address[bytes] = '\0';
    return static_cast<int>(bytes);
}

} // namespace legacy
} // namespace revive
