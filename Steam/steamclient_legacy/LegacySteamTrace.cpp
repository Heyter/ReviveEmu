#include "LegacySteamTrace.h"

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <set>
#include <string>

#include "LegacySteamRuntime.h"

namespace revive
{
namespace legacy
{

namespace
{

bool EnvEnabled(const char *value)
{
    if (!value || !*value)
        return false;

    return std::strcmp(value, "0") != 0 &&
           std::strcmp(value, "false") != 0 &&
           std::strcmp(value, "FALSE") != 0 &&
           std::strcmp(value, "off") != 0 &&
           std::strcmp(value, "OFF") != 0;
}

bool MarkFirst(const std::string &key)
{
    static std::mutex mutex;
    static std::set<std::string> seen;
    std::lock_guard<std::mutex> lock(mutex);
    return seen.insert(key).second;
}

} // namespace

bool AbiTraceEnabled()
{
    static const bool enabled = EnvEnabled(std::getenv("REVIVE_ABI_TRACE"));
    return enabled;
}

const char *AbiSupportString(AbiSupport support)
{
    switch (support)
    {
        case kAbiImplemented: return "implemented";
        case kAbiPartial: return "partial";
        case kAbiCompatibleNoop: return "compat_noop";
        case kAbiUnsupported: return "unsupported";
        default: return "unknown";
    }
}

void TraceAbiCall(const char *surface, const char *name, AbiSupport support)
{
    if (!AbiTraceEnabled())
        return;

    const char *safeSurface = surface ? surface : "unknown";
    const char *safeName = name ? name : "unknown";
    const std::string key = std::string("call:") + safeSurface + ":" + safeName;
    if (!MarkFirst(key))
        return;

    Log("ABI", "first_call surface=%s name=%s support=%s",
        safeSurface, safeName, AbiSupportString(support));
}

void TraceAbiInterfaceQuery(const char *owner, const char *requestedVersion, bool resolved)
{
    if (!AbiTraceEnabled())
        return;

    const char *safeOwner = owner ? owner : "unknown";
    const char *safeVersion = requestedVersion ? requestedVersion : "(null)";
    const std::string key = std::string("query:") + safeOwner + ":" + safeVersion +
                            (resolved ? ":1" : ":0");
    if (!MarkFirst(key))
        return;

    Log("ABI", "interface_query owner=%s version=%s resolved=%d",
        safeOwner, safeVersion, resolved ? 1 : 0);
}

} // namespace legacy
} // namespace revive
