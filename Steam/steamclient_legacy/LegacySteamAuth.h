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
    kSteam2RegistrationDuplicateSteamID,
    kSteam2RegistrationAuthCanceled
};

enum Steam2DisconnectResult
{
    kSteam2DisconnectRemoved = 0,
    kSteam2DisconnectCanceledPendingAuth,
    kSteam2DisconnectAlreadyAbsent,
    kSteam2DisconnectIdentityMismatch
};

struct Steam2AuthAttempt
{
    Steam2AuthAttempt();

    uint64 generation;
    bool createdReservation;
};

Steam2AuthAttempt BeginSteam2AuthLocked(RuntimeState &state, uint32 accountId);
Steam2RegistrationResult CompleteSteam2UserLocked(RuntimeState &state, uint32 accountId,
                                                   const Steam2AuthAttempt &attempt,
                                                   const auth::AuthTicketIdentity &identity,
                                                   uint64 *sessionGeneration);
void AbortSteam2AuthLocked(RuntimeState &state, uint32 accountId, const Steam2AuthAttempt &attempt,
                           const char *reason);
const char *Steam2RegistrationResultString(Steam2RegistrationResult result);
size_t RemoveSteam2UserLocked(RuntimeState &state, uint32 accountId, uint64 *removedGeneration);
Steam2DisconnectResult DisconnectSteam2UserLocked(RuntimeState &state, uint32 accountId,
                                                   const CSteamID &steamID, uint64 *expectedSteamID,
                                                   uint64 *removedGeneration);

} // namespace legacy
} // namespace revive
