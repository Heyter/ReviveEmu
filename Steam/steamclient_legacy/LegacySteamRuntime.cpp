#include "LegacySteamRuntime.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace revive
{
namespace legacy
{

namespace
{

bool ProductionLogComponent(const char *component)
{
    if (!component)
        return false;

    return std::strcmp(component, "Auth") == 0 ||
           std::strcmp(component, "Lifecycle") == 0 ||
           std::strcmp(component, "ABI") == 0;
}

bool LogEnabledForComponent(const char *component)
{
    const char *mode = std::getenv("REVIVE_LOG_MODE");
    if (!mode || !*mode || std::strcmp(mode, "diagnostic") == 0 || std::strcmp(mode, "verbose") == 0)
        return true;
    if (std::strcmp(mode, "off") == 0)
        return false;
    if (std::strcmp(mode, "production") == 0 || std::strcmp(mode, "normal") == 0)
        return ProductionLogComponent(component);

    // Unknown values deliberately fall back to diagnostic logging instead of
    // silently hiding operational information. start.sh validates production
    // deployments before the process reaches this point.
    return true;
}

} // namespace

Steam2AuthSession::Steam2AuthSession()
    : steamID(), ticketFingerprint(0), ticketType(0), generation(0),
      lifecycle(kClientLifecycleNew), identityBound(false)
{
}

MasterServerState::MasterServerState()
    : active(false),
      heartbeatInterval(-1),
      protocolVersion(0),
      dedicated(false),
      maxClients(0),
      passwordProtected(false),
      masterServerCount(0),
      shutdownNotified(false),
      heartbeatForced(false)
{
    regionName[0] = '\0';
    productName[0] = '\0';
    gameDescription[0] = '\0';
    std::memset(keyValues, 0, sizeof(keyValues));
    std::memset(masterServers, 0, sizeof(masterServers));
}

RuntimeState::RuntimeState()
    : callbackInFlight(false),
      currentCallbackUser(0),
      loggedOn(false),
      localIP(0),
      localPort(0),
      botCounter(1),
      nextClientGeneration(0)
{
}

RuntimeState &Runtime()
{
    static RuntimeState state;
    return state;
}

void Log(const char *component, const char *fmt, ...)
{
    if (!LogEnabledForComponent(component))
        return;

    static std::mutex logMutex;
    std::lock_guard<std::mutex> lock(logMutex);

    const char *env = std::getenv("REVIVE_STEAMCLIENT_LOG");
    const char *path = (env && *env) ? env : "revive_steamclient.log";
    FILE *f = std::fopen(path, "a");
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
    std::fprintf(f, "%s [ReviveEmu][%s] ", stamp, component ? component : "Legacy");

    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(f, fmt, ap);
    va_end(ap);
    std::fputc('\n', f);
    std::fclose(f);
}

const char *Steam2String(const CSteamID &steamID, char *buf, size_t bufSize)
{
    const uint32 account = steamID.GetAccountID();
    std::snprintf(buf, bufSize, "STEAM_0:%u:%u", account & 1u, account >> 1);
    return buf;
}

CSteamID MakeServerID()
{
    RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    uint32 account = state.localIP ^ (static_cast<uint32>(state.localPort) << 16) ^ 0x52455649u;
    if (account == 0)
        account = 0x539;
    return CSteamID(account, 0, k_EUniversePublic, k_EAccountTypeAnonGameServer);
}

} // namespace legacy
} // namespace revive
