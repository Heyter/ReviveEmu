#include "LegacySteamAuth.h"

#include "LegacySteamLifecycle.h"

namespace revive
{
namespace legacy
{

Steam2AuthAttempt::Steam2AuthAttempt()
    : generation(0), createdReservation(false), capacityExceeded(false)
{
}

Steam2AuthAttempt BeginSteam2AuthLocked(RuntimeState &state, uint32 accountId)
{
    Steam2AuthAttempt attempt;
    std::map<uint32, Steam2AuthSession>::const_iterator existing = state.steam2Users.find(accountId);
    if (existing != state.steam2Users.end())
    {
        attempt.generation = existing->second.generation;
        return attempt;
    }

    if (state.steam2Users.size() >= kMaxSteam2Sessions)
    {
        attempt.capacityExceeded = true;
        Log("Auth", "Steam2 auth reservation rejected account=%u sessions=%u limit=%u reason=session_capacity",
            accountId, static_cast<unsigned>(state.steam2Users.size()),
            static_cast<unsigned>(kMaxSteam2Sessions));
        return attempt;
    }

    ++state.nextClientGeneration;
    if (state.nextClientGeneration == 0)
        ++state.nextClientGeneration;

    Steam2AuthSession session;
    session.generation = state.nextClientGeneration;
    session.lifecycle = kClientLifecycleNew;
    state.steam2Users[accountId] = session;

    attempt.generation = session.generation;
    attempt.createdReservation = true;
    Log("Lifecycle", "created account=%u generation=%llu state=NEW reason=auth_begin",
        accountId, static_cast<unsigned long long>(attempt.generation));
    return attempt;
}

Steam2RegistrationResult CompleteSteam2UserLocked(RuntimeState &state, uint32 accountId,
                                                   const Steam2AuthAttempt &attempt,
                                                   const auth::AuthTicketIdentity &identity,
                                                   uint64 *sessionGeneration)
{
    std::map<uint32, Steam2AuthSession>::iterator accountIt = state.steam2Users.find(accountId);
    if (accountIt == state.steam2Users.end() || accountIt->second.generation != attempt.generation)
        return kSteam2RegistrationAuthCanceled;

    Steam2AuthSession &accountSession = accountIt->second;
    const uint64 steamID64 = identity.steamID.ConvertToUint64();
    if (accountSession.identityBound)
    {
        if (sessionGeneration)
            *sessionGeneration = accountSession.generation;
        return accountSession.steamID.ConvertToUint64() == steamID64
            ? kSteam2RegistrationIdempotent
            : kSteam2RegistrationAccountConflict;
    }

    for (std::map<uint32, Steam2AuthSession>::const_iterator it = state.steam2Users.begin();
         it != state.steam2Users.end(); ++it)
    {
        if (it->first == accountId || !it->second.identityBound)
            continue;
        if (it->second.steamID.ConvertToUint64() != steamID64)
            continue;

        const Steam2RegistrationResult result =
            identity.fingerprint != 0 && it->second.ticketFingerprint == identity.fingerprint
                ? kSteam2RegistrationActiveReplay
                : kSteam2RegistrationDuplicateSteamID;
        RemoveClientLifecycleLocked(state, accountId, attempt.generation,
                                    Steam2RegistrationResultString(result));
        return result;
    }

    accountSession.steamID = identity.steamID;
    accountSession.ticketFingerprint = identity.fingerprint;
    accountSession.ticketType = static_cast<uint32>(identity.type);
    accountSession.identityBound = true;
    if (!TransitionClientLifecycleLocked(state, accountId, attempt.generation,
                                         kClientLifecycleAuthPending, "ticket_validated"))
    {
        RemoveClientLifecycleLocked(state, accountId, attempt.generation, "auth_transition_failed");
        return kSteam2RegistrationAuthCanceled;
    }

    if (sessionGeneration)
        *sessionGeneration = attempt.generation;
    return kSteam2RegistrationAccepted;
}

void AbortSteam2AuthLocked(RuntimeState &state, uint32 accountId, const Steam2AuthAttempt &attempt,
                           const char *reason)
{
    if (!attempt.createdReservation)
        return;

    std::map<uint32, Steam2AuthSession>::iterator it = state.steam2Users.find(accountId);
    if (it == state.steam2Users.end() || it->second.generation != attempt.generation || it->second.identityBound)
        return;

    RemoveClientLifecycleLocked(state, accountId, attempt.generation, reason ? reason : "auth_abort");
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
        case kSteam2RegistrationAuthCanceled: return "auth_canceled";
        case kSteam2RegistrationCallbackQueueFull: return "callback_queue_full";
        default: return "unknown";
    }
}

size_t RemoveSteam2UserLocked(RuntimeState &state, uint32 accountId, uint64 *removedGeneration,
                              const char *reason)
{
    std::map<uint32, Steam2AuthSession>::const_iterator it = state.steam2Users.find(accountId);
    if (it == state.steam2Users.end())
        return 0;
    const uint64 generation = it->second.generation;
    if (removedGeneration)
        *removedGeneration = generation;
    return RemoveClientLifecycleLocked(state, accountId, generation, reason ? reason : "remove_user_connect");
}

Steam2DisconnectResult DisconnectSteam2UserLocked(RuntimeState &state, uint32 accountId,
                                                   const CSteamID &steamID, uint64 *expectedSteamID,
                                                   uint64 *removedGeneration)
{
    std::map<uint32, Steam2AuthSession>::iterator it = state.steam2Users.find(accountId);
    if (it == state.steam2Users.end())
        return kSteam2DisconnectAlreadyAbsent;

    const uint64 generation = it->second.generation;
    if (removedGeneration)
        *removedGeneration = generation;

    if (!it->second.identityBound)
    {
        if (expectedSteamID)
            *expectedSteamID = 0;
        RemoveClientLifecycleLocked(state, accountId, generation, "disconnect_during_auth");
        return kSteam2DisconnectCanceledPendingAuth;
    }

    const uint64 expected = it->second.steamID.ConvertToUint64();
    if (expectedSteamID)
        *expectedSteamID = expected;

    if (expected != steamID.ConvertToUint64())
        return kSteam2DisconnectIdentityMismatch;

    RemoveClientLifecycleLocked(state, accountId, generation, "user_disconnect");
    return kSteam2DisconnectRemoved;
}

} // namespace legacy
} // namespace revive
