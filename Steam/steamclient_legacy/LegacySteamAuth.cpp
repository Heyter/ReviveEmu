#include "LegacySteamAuth.h"

namespace revive
{
namespace legacy
{

Steam2RegistrationResult RegisterSteam2UserLocked(RuntimeState &state, uint32 accountId,
                                                   const auth::AuthTicketIdentity &identity)
{
    const uint64 steamID64 = identity.steamID.ConvertToUint64();
    std::map<uint32, Steam2AuthSession>::const_iterator accountIt = state.steam2Users.find(accountId);
    if (accountIt != state.steam2Users.end())
    {
        // Preserve the accepted M3.2 idempotency contract: the engine may submit
        // the same logical identity again for the same account while it is active.
        return accountIt->second.steamID.ConvertToUint64() == steamID64
            ? kSteam2RegistrationIdempotent
            : kSteam2RegistrationAccountConflict;
    }

    for (std::map<uint32, Steam2AuthSession>::const_iterator it = state.steam2Users.begin();
         it != state.steam2Users.end(); ++it)
    {
        if (it->second.steamID.ConvertToUint64() != steamID64)
            continue;

        if (identity.fingerprint != 0 && it->second.ticketFingerprint == identity.fingerprint)
            return kSteam2RegistrationActiveReplay;
        return kSteam2RegistrationDuplicateSteamID;
    }

    Steam2AuthSession session;
    session.steamID = identity.steamID;
    session.ticketFingerprint = identity.fingerprint;
    session.ticketType = static_cast<uint32>(identity.type);
    state.steam2Users[accountId] = session;
    return kSteam2RegistrationAccepted;
}

const char *Steam2RegistrationResultString(Steam2RegistrationResult result)
{
    switch (result)
    {
        case kSteam2RegistrationAccepted: return "accepted";
        case kSteam2RegistrationIdempotent: return "idempotent";
        case kSteam2RegistrationAccountConflict: return "account_conflict";
        case kSteam2RegistrationActiveReplay: return "active_ticket_replay";
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
    std::map<uint32, Steam2AuthSession>::iterator it = state.steam2Users.find(accountId);
    if (it == state.steam2Users.end())
        return kSteam2DisconnectAlreadyAbsent;

    const uint64 expected = it->second.steamID.ConvertToUint64();
    if (expectedSteamID)
        *expectedSteamID = expected;

    if (expected != steamID.ConvertToUint64())
        return kSteam2DisconnectIdentityMismatch;

    state.steam2Users.erase(it);
    return kSteam2DisconnectRemoved;
}

} // namespace legacy
} // namespace revive
