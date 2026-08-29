#include "LegacySteamAuth.h"

namespace revive
{
namespace legacy
{

Steam2RegistrationResult RegisterSteam2UserLocked(RuntimeState &state, uint32 accountId, const CSteamID &steamID)
{
    const uint64 steamID64 = steamID.ConvertToUint64();
    std::map<uint32, CSteamID>::const_iterator accountIt = state.steam2Users.find(accountId);
    if (accountIt != state.steam2Users.end())
    {
        return accountIt->second.ConvertToUint64() == steamID64
            ? kSteam2RegistrationIdempotent
            : kSteam2RegistrationAccountConflict;
    }

    for (std::map<uint32, CSteamID>::const_iterator it = state.steam2Users.begin();
         it != state.steam2Users.end(); ++it)
    {
        if (it->second.ConvertToUint64() == steamID64)
            return kSteam2RegistrationDuplicateSteamID;
    }

    state.steam2Users[accountId] = steamID;
    return kSteam2RegistrationAccepted;
}

const char *Steam2RegistrationResultString(Steam2RegistrationResult result)
{
    switch (result)
    {
        case kSteam2RegistrationAccepted: return "accepted";
        case kSteam2RegistrationIdempotent: return "idempotent";
        case kSteam2RegistrationAccountConflict: return "account_conflict";
        case kSteam2RegistrationDuplicateSteamID: return "duplicate_steamid";
        default: return "unknown";
    }
}

size_t RemoveSteam2UserLocked(RuntimeState &state, uint32 accountId)
{
    return state.steam2Users.erase(accountId);
}

Steam2DisconnectResult DisconnectSteam2UserLocked(RuntimeState &state, uint32 accountId,
                                                   const CSteamID &steamID, uint64 *expectedSteamID)
{
    std::map<uint32, CSteamID>::iterator it = state.steam2Users.find(accountId);
    if (it == state.steam2Users.end())
        return kSteam2DisconnectAlreadyAbsent;

    const uint64 expected = it->second.ConvertToUint64();
    if (expectedSteamID)
        *expectedSteamID = expected;

    if (expected != steamID.ConvertToUint64())
        return kSteam2DisconnectIdentityMismatch;

    state.steam2Users.erase(it);
    return kSteam2DisconnectRemoved;
}

} // namespace legacy
} // namespace revive
