#pragma once

#include <cstddef>
#include <cstdint>

namespace revive
{
namespace legacy
{

struct RuntimeState;
struct QueuedCallback;

enum ClientLifecycleState
{
    kClientLifecycleNew = 0,
    kClientLifecycleAuthPending,
    kClientLifecycleAuthenticated,
    kClientLifecycleActive,
    kClientLifecycleDisconnecting,
    kClientLifecycleRemoved
};

const char *ClientLifecycleStateString(ClientLifecycleState state);

bool TransitionClientLifecycleLocked(RuntimeState &state, uint32_t accountId, uint64_t generation,
                                     ClientLifecycleState nextState, const char *reason);
size_t RemoveClientLifecycleLocked(RuntimeState &state, uint32_t accountId, uint64_t generation,
                                   const char *reason);
void ApplyAuthCallbackLifecycleLocked(RuntimeState &state, const QueuedCallback &callback);

} // namespace legacy
} // namespace revive
