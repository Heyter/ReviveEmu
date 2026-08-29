#pragma once

#include "LegacySteamRuntime.h"

namespace revive
{
namespace legacy
{

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
    virtual bool Obsolete_GSSetStatus(int32 appIdServed, uint32 serverFlags, int players, int maxPlayers,
                                      int botPlayers, int gamePort, const char *serverName,
                                      const char *gameDir, const char *mapName, const char *version);
    virtual bool GSUpdateStatus(int players, int maxPlayers, int botPlayers, const char *serverName, const char *mapName);
    virtual bool BSecure();
    virtual CSteamID GetSteamID();
    virtual bool GSSetServerType(int32 gameAppId, uint32 serverFlags, uint32 gameIP, uint32 gamePort,
                                 const char *gameDir, const char *version);
    virtual bool GSSetServerType2(int32 gameAppId, uint32 serverFlags, uint32 gameIP,
                                  uint16 gamePort, uint16 spectatorPort, uint16 queryPort,
                                  const char *gameDir, const char *version, bool lanMode);
    virtual bool GSUpdateStatus2(int players, int maxPlayers, int botPlayers,
                                 const char *serverName, const char *spectatorServerName, const char *mapName);
    virtual bool GSCreateUnauthenticatedUser(CSteamID *steamID);
    virtual bool GSSetUserData(CSteamID steamID, const char *playerName, uint32 score);
    virtual void GSUpdateSpectatorPort(uint16 spectatorPort);
    virtual void GSSetGameType(const char *gameType);
};

class CSteamUtils001
{
public:
    virtual uint32 GetSecondsSinceAppActive();
    virtual uint32 GetSecondsSinceComputerActive();
    virtual EUniverse GetConnectedUniverse();
    virtual uint32 GetServerRealTime();
};

class CSteamMasterServerUpdater001
{
public:
    virtual void SetActive(bool active);
    virtual void SetHeartbeatInterval(int interval);
    virtual bool HandleIncomingPacket(const void *data, int size, uint32 ip, uint16 port);
    virtual int GetNextOutgoingPacket(void *data, int maxSize, uint32 *ip, uint16 *port);
    virtual void SetBasicServerData(uint16 protocolVersion, bool dedicated, const char *region,
                                    const char *productName, uint16 maxClients, bool passwordProtected,
                                    const char *gameDescription);
    virtual void ClearAllKeyValues();
    virtual void SetKeyValue(const char *key, const char *value);
    virtual void NotifyShutdown();
    virtual bool WasRestartRequested();
    virtual void ForceHeartbeat();
    virtual bool AddMasterServer(const char *serverAddress);
    virtual bool RemoveMasterServer(const char *serverAddress);
    virtual int GetNumMasterServers();
    virtual int GetMasterServerAddress(int serverIndex, char *address, int addressSize);
};

class CSteamClient006
{
public:
    virtual HSteamPipe CreateSteamPipe();
    virtual bool BReleaseSteamPipe(HSteamPipe pipe);
    virtual HSteamUser CreateGlobalUser(HSteamPipe *pipe);
    virtual HSteamUser ConnectToGlobalUser(HSteamPipe pipe);
    virtual HSteamUser CreateLocalUser(HSteamPipe *pipe);
    virtual void ReleaseUser(HSteamPipe pipe, HSteamUser user);
    virtual void *GetISteamUser(HSteamUser user, HSteamPipe pipe, const char *version);
    virtual void *GetIVAC(HSteamUser user);
    virtual void *GetISteamGameServer(HSteamUser user, HSteamPipe pipe, const char *version);
    virtual void SetLocalIPBinding(uint32 ip, uint16 port);
    virtual const char *GetUniverseName(EUniverse universe);
    virtual void *GetISteamFriends(HSteamUser user, HSteamPipe pipe, const char *version);
    virtual void *GetISteamUtils(HSteamPipe pipe, const char *version);
    virtual void *GetISteamBilling(HSteamUser user, HSteamPipe pipe, const char *version);
    virtual void *GetISteamMatchMaking(HSteamUser user, HSteamPipe pipe, const char *version);
    virtual void *GetISteamContentServer(HSteamUser user, HSteamPipe pipe, const char *version);
    virtual void *GetISteamApps(HSteamUser user, HSteamPipe pipe, const char *version);
    virtual void *GetISteamMasterServerUpdater(HSteamUser user, HSteamPipe pipe, const char *version);
    virtual void *GetISteamMatchMakingServers(HSteamUser user, HSteamPipe pipe, const char *version);
    virtual void RunFrame();
    virtual uint32 GetIPCCallCount();
};

CSteamClient006 &SteamClient();
CSteamGameServer002 &SteamGameServer();
CSteamUtils001 &SteamUtils();
CSteamMasterServerUpdater001 &SteamMasterServerUpdater();
CSteamGameServer002 *GameServerFromHandle(void *handle);

} // namespace legacy
} // namespace revive
