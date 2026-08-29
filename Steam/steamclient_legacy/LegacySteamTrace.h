#pragma once

namespace revive
{
namespace legacy
{

enum AbiSupport
{
    kAbiImplemented,
    kAbiPartial,
    kAbiCompatibleNoop,
    kAbiUnsupported
};

bool AbiTraceEnabled();
const char *AbiSupportString(AbiSupport support);
void TraceAbiCall(const char *surface, const char *name, AbiSupport support);
void TraceAbiInterfaceQuery(const char *owner, const char *requestedVersion, bool resolved);

} // namespace legacy
} // namespace revive
