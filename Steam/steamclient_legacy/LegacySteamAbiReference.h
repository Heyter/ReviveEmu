#pragma once

// Steamworks-era vtable slot map used by CS:S Build 4100 compatibility.
// The order is cross-checked against SSE_PDK/Open Steamworks 1.43 and the
// matching legacy interface declarations in gbe_fork.

namespace revive
{
namespace legacy
{
enum SteamClient006Slot
{
    kClient006CreateSteamPipe = 0,
    kClient006BReleaseSteamPipe = 1,
    kClient006CreateGlobalUser = 2,
    kClient006ConnectToGlobalUser = 3,
    kClient006CreateLocalUser = 4,
    kClient006ReleaseUser = 5,
    kClient006GetISteamUser = 6,
    kClient006GetIVAC = 7,
    kClient006GetISteamGameServer = 8,
    kClient006SetLocalIPBinding = 9,
    kClient006GetUniverseName = 10,
    kClient006GetISteamFriends = 11,
    kClient006GetISteamUtils = 12,
    kClient006GetISteamBilling = 13,
    kClient006GetISteamMatchmaking = 14,
    kClient006GetISteamContentServer = 15,
    kClient006GetISteamApps = 16,
    kClient006GetISteamMasterServerUpdater = 17,
    kClient006GetISteamMatchmakingServers = 18,
    kClient006RunFrame = 19,
    kClient006GetIPCCallCount = 20,
    kClient006SlotCount = 21
};

enum SteamGameServer002Slot
{
    kGameServer002LogOn = 0,
    kGameServer002LogOff = 1,
    kGameServer002BLoggedOn = 2,
    kGameServer002SetSpawnCount = 3,
    kGameServer002GetSteam2EncryptionKey = 4,
    kGameServer002SendSteam2UserConnect = 5,
    kGameServer002SendSteam3UserConnect = 6,
    kGameServer002RemoveUserConnect = 7,
    kGameServer002SendUserDisconnect = 8,
    kGameServer002SendUserStatusResponse = 9,
    kGameServer002ObsoleteSetStatus = 10,
    kGameServer002UpdateStatus = 11,
    kGameServer002BSecure = 12,
    kGameServer002GetSteamID = 13,
    kGameServer002SetServerType = 14,
    kGameServer002SetServerType2 = 15,
    kGameServer002UpdateStatus2 = 16,
    kGameServer002CreateUnauthenticatedUser = 17,
    kGameServer002SetUserData = 18,
    kGameServer002UpdateSpectatorPort = 19,
    kGameServer002SetGameType = 20,
    kGameServer002SlotCount = 21
};

enum SteamUtils001Slot
{
    kUtils001GetSecondsSinceAppActive = 0,
    kUtils001GetSecondsSinceComputerActive = 1,
    kUtils001GetConnectedUniverse = 2,
    kUtils001GetServerRealTime = 3,
    kUtils001SlotCount = 4
};

enum SteamMasterServerUpdater001Slot
{
    kMaster001SetActive = 0,
    kMaster001SetHeartbeatInterval = 1,
    kMaster001HandleIncomingPacket = 2,
    kMaster001GetNextOutgoingPacket = 3,
    kMaster001SetBasicServerData = 4,
    kMaster001ClearAllKeyValues = 5,
    kMaster001SetKeyValue = 6,
    kMaster001NotifyShutdown = 7,
    kMaster001WasRestartRequested = 8,
    kMaster001ForceHeartbeat = 9,
    kMaster001AddMasterServer = 10,
    kMaster001RemoveMasterServer = 11,
    kMaster001GetNumMasterServers = 12,
    kMaster001GetMasterServerAddress = 13,
    kMaster001SlotCount = 14
};

} // namespace legacy
} // namespace revive
