#pragma once

#include "LegacySteamRuntime.h"
#include "auth/LegacyAuthTicket.h"

namespace revive
{
namespace legacy
{

enum Steam2RegistrationResult
{
    kSteam2RegistrationAccepted = 0,
    kSteam2RegistrationIdempotent,
    kSteam2RegistrationAccountConflict,
    kSteam2RegistrationActiveReplay,
    kSteam2RegistrationDuplicateSteamID
};

enum Steam2DisconnectResult
{
    kSteam2DisconnectRemoved = 0,
    kSteam2DisconnectAlreadyAbsent,
    kSteam2DisconnectIdentityMismatch
};

Steam2RegistrationResult RegisterSteam2UserLocked(RuntimeState &state, uint32 accountId,
                                                   const auth::AuthTicketIdentity &identity);
const char *Steam2RegistrationResultString(Steam2RegistrationResult result);
size_t RemoveSteam2UserLocked(RuntimeState &state, uint32 accountId);
Steam2DisconnectResult DisconnectSteam2UserLocked(RuntimeState &state, uint32 accountId,
                                                   const CSteamID &steamID, uint64 *expectedSteamID);

} // namespace legacy
} // namespace revive
