#include "LegacySteamLifecycle.h"

#include "LegacySteamRuntime.h"

namespace revive
{
namespace legacy
{

namespace
{

bool IsAllowedTransition(ClientLifecycleState from, ClientLifecycleState to)
{
    switch (from)
    {
        case kClientLifecycleNew:
            return to == kClientLifecycleAuthPending || to == kClientLifecycleDisconnecting;
        case kClientLifecycleAuthPending:
            return to == kClientLifecycleAuthenticated || to == kClientLifecycleDisconnecting;
        case kClientLifecycleAuthenticated:
            return to == kClientLifecycleActive || to == kClientLifecycleDisconnecting;
        case kClientLifecycleActive:
            return to == kClientLifecycleDisconnecting;
        case kClientLifecycleDisconnecting:
            return to == kClientLifecycleRemoved;
        case kClientLifecycleRemoved:
        default:
            return false;
    }
}

} // namespace

const char *ClientLifecycleStateString(ClientLifecycleState state)
{
    switch (state)
    {
        case kClientLifecycleNew: return "NEW";
        case kClientLifecycleAuthPending: return "AUTH_PENDING";
        case kClientLifecycleAuthenticated: return "AUTHENTICATED";
        case kClientLifecycleActive: return "ACTIVE";
        case kClientLifecycleDisconnecting: return "DISCONNECTING";
        case kClientLifecycleRemoved: return "REMOVED";
        default: return "UNKNOWN";
    }
}

bool TransitionClientLifecycleLocked(RuntimeState &state, uint32_t accountId, uint64_t generation,
                                     ClientLifecycleState nextState, const char *reason)
{
    std::map<uint32, Steam2AuthSession>::iterator it = state.steam2Users.find(accountId);
    if (it == state.steam2Users.end())
    {
        Log("Lifecycle", "transition ignored account=%u generation=%llu target=%s reason=session_missing source=%s",
            accountId, static_cast<unsigned long long>(generation), ClientLifecycleStateString(nextState),
            reason ? reason : "unspecified");
        return false;
    }

    Steam2AuthSession &session = it->second;
    if (session.generation != generation)
    {
        Log("Lifecycle", "transition ignored account=%u generation=%llu current_generation=%llu target=%s reason=stale_generation source=%s",
            accountId, static_cast<unsigned long long>(generation),
            static_cast<unsigned long long>(session.generation), ClientLifecycleStateString(nextState),
            reason ? reason : "unspecified");
        return false;
    }

    const ClientLifecycleState previous = session.lifecycle;
    if (previous == nextState)
        return true;

    if (!IsAllowedTransition(previous, nextState))
    {
        Log("Lifecycle", "transition rejected account=%u generation=%llu state=%s->%s reason=invalid_transition source=%s",
            accountId, static_cast<unsigned long long>(generation), ClientLifecycleStateString(previous),
            ClientLifecycleStateString(nextState), reason ? reason : "unspecified");
        return false;
    }

    session.lifecycle = nextState;
    Log("Lifecycle", "transition account=%u generation=%llu state=%s->%s reason=%s",
        accountId, static_cast<unsigned long long>(generation), ClientLifecycleStateString(previous),
        ClientLifecycleStateString(nextState), reason ? reason : "unspecified");
    return true;
}

size_t RemoveClientLifecycleLocked(RuntimeState &state, uint32_t accountId, uint64_t generation,
                                   const char *reason)
{
    std::map<uint32, Steam2AuthSession>::iterator it = state.steam2Users.find(accountId);
    if (it == state.steam2Users.end() || (generation != 0 && it->second.generation != generation))
        return 0;

    const uint64_t currentGeneration = it->second.generation;
    if (it->second.lifecycle != kClientLifecycleDisconnecting)
    {
        if (!TransitionClientLifecycleLocked(state, accountId, currentGeneration,
                                             kClientLifecycleDisconnecting, reason))
            return 0;
    }

    if (!TransitionClientLifecycleLocked(state, accountId, currentGeneration,
                                         kClientLifecycleRemoved, reason))
        return 0;

    state.steam2Users.erase(accountId);
    return 1;
}

void ApplyAuthCallbackLifecycleLocked(RuntimeState &state, const QueuedCallback &callback)
{
    if (!callback.authCallback || callback.generation == 0)
        return;

    if (callback.callback == kCallbackGSClientSteam2Accept)
    {
        TransitionClientLifecycleLocked(state, callback.accountId, callback.generation,
                                        kClientLifecycleAuthenticated, "callback_205_consumed");
    }
    else if (callback.callback == kCallbackGSClientApprove)
    {
        TransitionClientLifecycleLocked(state, callback.accountId, callback.generation,
                                        kClientLifecycleActive, "callback_201_consumed");
    }
}

} // namespace legacy
} // namespace revive
