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
    std::map<uint32, CSteamID> steam2Users;
    uint32 botCounter;
};

RuntimeState &Runtime();

void Log(const char *component, const char *fmt, ...);
const char *Steam2String(const CSteamID &steamID, char *buf, size_t bufSize);
CSteamID MakeServerID();

} // namespace legacy
} // namespace revive
