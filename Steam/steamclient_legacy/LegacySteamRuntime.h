#pragma once

#include <cstddef>
#include <deque>
#include <map>
#include <mutex>
#include <vector>

#include "../steam/SteamCommon.h"
#include "../steam/RevCommon.h"
#define INCLUDED_STEAM2_USERID_STRUCTS
#include "../steam/Steam3ID.h"

namespace revive
{
namespace legacy
{

typedef int HSteamPipe;
typedef int HSteamUser;

static const HSteamPipe kPipe = 1;
static const HSteamUser kUser = 1;
static const int kCallbackSteamServersConnected = 101;
static const int kCallbackGSClientApprove = 201;
static const int kCallbackGSClientSteam2Accept = 205;

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
    uint32 accountId;
    uint64 steamID;
    bool authCallback;
};

struct MasterKeyValue
{
    bool used;
    char key[64];
    char value[128];
};

struct MasterServerState
{
    MasterServerState();

    bool active;
    int heartbeatInterval;
    uint16 protocolVersion;
    bool dedicated;
    char regionName[64];
    char productName[64];
    uint16 maxClients;
    bool passwordProtected;
    char gameDescription[128];
    MasterKeyValue keyValues[64];
    char masterServers[16][128];
    size_t masterServerCount;
    bool shutdownNotified;
    bool heartbeatForced;
};

struct Steam2AuthSession
{
    Steam2AuthSession();

    CSteamID steamID;
    uint64 ticketFingerprint;
    uint32 ticketType;
};

struct RuntimeState
{
    RuntimeState();

    std::mutex mutex;
    std::deque<QueuedCallback> callbacks;
    bool callbackInFlight;
    HSteamUser currentCallbackUser;
    bool loggedOn;
    uint32 localIP;
    uint16 localPort;
    std::map<uint32, Steam2AuthSession> steam2Users;
    MasterServerState masterServer;
    uint32 botCounter;
};

RuntimeState &Runtime();

void Log(const char *component, const char *fmt, ...);
const char *Steam2String(const CSteamID &steamID, char *buf, size_t bufSize);
CSteamID MakeServerID();

} // namespace legacy
} // namespace revive
