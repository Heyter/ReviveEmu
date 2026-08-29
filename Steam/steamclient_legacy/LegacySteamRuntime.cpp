#include "LegacySteamRuntime.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace revive
{
namespace legacy
{

RuntimeState::RuntimeState()
    : callbackInFlight(false),
      currentCallbackUser(0),
      loggedOn(false),
      localIP(0),
      localPort(0),
      botCounter(1)
{
}

RuntimeState &Runtime()
{
    static RuntimeState state;
    return state;
}

void Log(const char *component, const char *fmt, ...)
{
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
