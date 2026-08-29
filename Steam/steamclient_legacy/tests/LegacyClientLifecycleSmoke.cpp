#include <cstdio>
#include <cstdlib>

#include "../LegacySteamAuth.h"
#include "../LegacySteamLifecycle.h"
#include "../LegacySteamRuntime.h"

namespace
{

using namespace revive::legacy;

void Fail(const char *message)
{
    std::fprintf(stderr, "client lifecycle smoke FAIL: %s\n", message ? message : "unknown error");
    std::exit(1);
}

void Check(bool condition, const char *message)
{
    if (!condition)
        Fail(message);
}

auth::AuthTicketIdentity MakeIdentity(uint64 steamID64, uint64 fingerprint)
{
    auth::AuthTicketIdentity identity;
    identity.type = auth::kAuthTicketClassicRevEmu;
    identity.steamID = CSteamID(steamID64);
    identity.fingerprint = fingerprint;
    return identity;
}

QueuedCallback AuthCallback(int callback, uint32 accountId, uint64 steamID, uint64 generation)
{
    QueuedCallback item;
    item.user = kUser;
    item.callback = callback;
    item.accountId = accountId;
    item.steamID = steamID;
    item.authCallback = true;
    item.generation = generation;
    return item;
}

ClientLifecycleState StateOf(const RuntimeState &state, uint32 accountId)
{
    std::map<uint32, Steam2AuthSession>::const_iterator it = state.steam2Users.find(accountId);
    Check(it != state.steam2Users.end(), "expected lifecycle session is missing");
    return it->second.lifecycle;
}

} // namespace

int main()
{
    RuntimeState state;
    const uint32 account = 700;
    const uint64 steamID = 76561199000000700ULL;
    const auth::AuthTicketIdentity identity = MakeIdentity(steamID, 0x1111222233334444ULL);

    Steam2AuthAttempt first = BeginSteam2AuthLocked(state, account);
    Check(first.createdReservation && first.generation != 0, "NEW reservation was not created");
    Check(StateOf(state, account) == kClientLifecycleNew, "initial lifecycle state must be NEW");

    uint64 generation = 0;
    Check(CompleteSteam2UserLocked(state, account, first, identity, &generation) == kSteam2RegistrationAccepted,
          "valid ticket did not enter AUTH_PENDING");
    Check(generation == first.generation, "accepted generation mismatch");
    Check(StateOf(state, account) == kClientLifecycleAuthPending, "accepted auth must be AUTH_PENDING");

    QueuedCallback callback201Early = AuthCallback(kCallbackGSClientApprove, account, steamID, generation);
    ApplyAuthCallbackLifecycleLocked(state, callback201Early);
    Check(StateOf(state, account) == kClientLifecycleAuthPending,
          "callback 201 must not skip AUTHENTICATED");

    QueuedCallback callback205 = AuthCallback(kCallbackGSClientSteam2Accept, account, steamID, generation);
    ApplyAuthCallbackLifecycleLocked(state, callback205);
    Check(StateOf(state, account) == kClientLifecycleAuthenticated,
          "callback 205 must advance AUTH_PENDING -> AUTHENTICATED");

    QueuedCallback callback201 = AuthCallback(kCallbackGSClientApprove, account, steamID, generation);
    ApplyAuthCallbackLifecycleLocked(state, callback201);
    Check(StateOf(state, account) == kClientLifecycleActive,
          "callback 201 must advance AUTHENTICATED -> ACTIVE");

    uint64 expected = 0;
    uint64 removedGeneration = 0;
    Check(DisconnectSteam2UserLocked(state, account, identity.steamID, &expected, &removedGeneration)
              == kSteam2DisconnectRemoved,
          "ACTIVE disconnect failed");
    Check(expected == steamID && removedGeneration == generation, "disconnect identity/generation mismatch");
    Check(state.steam2Users.find(account) == state.steam2Users.end(), "REMOVED session leaked from runtime");
    Check(DisconnectSteam2UserLocked(state, account, identity.steamID, &expected, &removedGeneration)
              == kSteam2DisconnectAlreadyAbsent,
          "double disconnect must be idempotent");

    const uint32 pendingAccount = 701;
    Steam2AuthAttempt canceled = BeginSteam2AuthLocked(state, pendingAccount);
    Check(StateOf(state, pendingAccount) == kClientLifecycleNew, "pending auth did not begin at NEW");
    Check(DisconnectSteam2UserLocked(state, pendingAccount, identity.steamID, &expected, &removedGeneration)
              == kSteam2DisconnectCanceledPendingAuth,
          "disconnect during auth did not cancel NEW reservation");
    Check(CompleteSteam2UserLocked(state, pendingAccount, canceled, identity, &generation)
              == kSteam2RegistrationAuthCanceled,
          "canceled auth attempt resurrected a removed session");

    // Reuse the same accountId immediately. A stale callback from the canceled
    // generation must not advance the new generation.
    Steam2AuthAttempt reused = BeginSteam2AuthLocked(state, pendingAccount);
    Check(reused.generation != canceled.generation, "accountId reuse did not allocate a new generation");
    Check(CompleteSteam2UserLocked(state, pendingAccount, reused, identity, &generation)
              == kSteam2RegistrationAccepted,
          "reused accountId could not authenticate");
    Check(StateOf(state, pendingAccount) == kClientLifecycleAuthPending,
          "reused accountId must start at AUTH_PENDING");

    QueuedCallback stale205 = AuthCallback(kCallbackGSClientSteam2Accept, pendingAccount, steamID,
                                           canceled.generation);
    ApplyAuthCallbackLifecycleLocked(state, stale205);
    Check(StateOf(state, pendingAccount) == kClientLifecycleAuthPending,
          "stale callback advanced a reused accountId generation");

    QueuedCallback fresh205 = AuthCallback(kCallbackGSClientSteam2Accept, pendingAccount, steamID, generation);
    ApplyAuthCallbackLifecycleLocked(state, fresh205);
    Check(StateOf(state, pendingAccount) == kClientLifecycleAuthenticated,
          "fresh callback 205 did not authenticate reused accountId");
    QueuedCallback fresh201 = AuthCallback(kCallbackGSClientApprove, pendingAccount, steamID, generation);
    ApplyAuthCallbackLifecycleLocked(state, fresh201);
    Check(StateOf(state, pendingAccount) == kClientLifecycleActive,
          "fresh callback 201 did not activate reused accountId");

    CSteamID wrongSteamID(steamID + 2);
    Check(DisconnectSteam2UserLocked(state, pendingAccount, wrongSteamID, &expected, &removedGeneration)
              == kSteam2DisconnectIdentityMismatch,
          "mismatched disconnect must reject");
    Check(StateOf(state, pendingAccount) == kClientLifecycleActive,
          "mismatched disconnect erased ACTIVE session");
    Check(DisconnectSteam2UserLocked(state, pendingAccount, identity.steamID, &expected, &removedGeneration)
              == kSteam2DisconnectRemoved,
          "final reused-account cleanup failed");

    std::puts("[PASS] NEW -> AUTH_PENDING -> AUTHENTICATED -> ACTIVE lifecycle");
    std::puts("[PASS] disconnect-during-auth cancellation");
    std::puts("[PASS] generation-safe stale callback isolation");
    std::puts("[PASS] double disconnect and accountId reuse");
    std::puts("M3.7 client lifecycle smoke PASS");
    return 0;
}
