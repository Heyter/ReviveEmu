#include "LegacySteamCallbacks.h"

#include "LegacySteamLifecycle.h"

namespace revive
{
namespace legacy
{

#pragma pack(push, 8)
struct GSClientApprovePayload
{
    CSteamID steamID;
    CSteamID ownerSteamID;
};

struct GSClientSteam2AcceptPayload
{
    uint32 userID;
    uint64 steamID;
};
#pragma pack(pop)

static_assert(sizeof(CSteamID) == 8, "CSteamID ABI must be 8 bytes");
static_assert(sizeof(GSClientApprovePayload) == 16, "GSClientApprove callback payload ABI must be 16 bytes");
static_assert(sizeof(void *) == 4 || sizeof(void *) == 8, "unsupported pointer size");
static_assert(sizeof(CallbackMsg_t) == (sizeof(void *) == 4 ? 16u : 24u),
              "CallbackMsg_t ABI layout mismatch");
static_assert(sizeof(void *) != 4 || offsetof(CallbackMsg_t, m_pubParam) == 8,
              "CallbackMsg_t parameter pointer offset must be 8 on legacy x86");
static_assert(sizeof(void *) != 4 || offsetof(CallbackMsg_t, m_cubParam) == 12,
              "CallbackMsg_t parameter size offset must be 12 on legacy x86");
static_assert(sizeof(GSClientSteam2AcceptPayload) == (sizeof(void *) == 4 ? 12u : 16u),
              "GSClientSteam2Accept callback payload ABI mismatch");

bool QueueCallbackLocked(RuntimeState &state, int callback, const void *payload, size_t payloadSize,
                         uint32 accountId, uint64 steamID, bool authCallback, uint64 generation)
{
    if (payloadSize > kMaxCallbackPayloadBytes || (payloadSize != 0 && !payload))
    {
        Log("Callback", "callback rejected id=%d size=%u reason=invalid_payload", callback,
            static_cast<unsigned>(payloadSize));
        return false;
    }
    if (state.callbacks.size() >= kMaxQueuedCallbacks)
    {
        Log("Callback", "callback rejected id=%d size=%u queued=%u limit=%u reason=queue_full", callback,
            static_cast<unsigned>(payloadSize), static_cast<unsigned>(state.callbacks.size()),
            static_cast<unsigned>(kMaxQueuedCallbacks));
        return false;
    }

    QueuedCallback item;
    item.user = kUser;
    item.callback = callback;
    item.accountId = accountId;
    item.steamID = steamID;
    item.authCallback = authCallback;
    item.generation = generation;
    if (payload && payloadSize)
    {
        const uint8 *p = static_cast<const uint8 *>(payload);
        item.payload.assign(p, p + payloadSize);
    }

    state.callbacks.push_back(item);
    Log("Callback", "callback queued id=%d size=%u", callback, static_cast<unsigned>(payloadSize));
    return true;
}

bool QueueCallback(int callback, const void *payload, size_t payloadSize)
{
    RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    return QueueCallbackLocked(state, callback, payload, payloadSize, 0, 0, false, 0);
}

bool QueueClientApprove(const CSteamID &steamID)
{
    GSClientApprovePayload payload;
    payload.steamID = steamID;
    payload.ownerSteamID = steamID;
    return QueueCallback(kCallbackGSClientApprove, &payload, sizeof(payload));
}

bool QueueSteam2AuthCallbacksLocked(RuntimeState &state, uint32 accountId, const CSteamID &steamID, uint64 generation)
{
    // A Steam2 authentication is only useful if both legacy callbacks can be
    // queued. Reserve capacity for the pair before pushing either item so an
    // abusive queue cannot leave a half-delivered 205 -> 201 transaction.
    if (state.callbacks.size() > kMaxQueuedCallbacks - 2)
    {
        Log("Callback", "auth callback pair rejected account=%u generation=%llu queued=%u limit=%u reason=queue_full",
            accountId, static_cast<unsigned long long>(generation),
            static_cast<unsigned>(state.callbacks.size()), static_cast<unsigned>(kMaxQueuedCallbacks));
        return false;
    }

    const uint64 steamID64 = steamID.ConvertToUint64();

    GSClientSteam2AcceptPayload acceptPayload;
    acceptPayload.userID = accountId;
    acceptPayload.steamID = steamID64;
    if (!QueueCallbackLocked(state, kCallbackGSClientSteam2Accept, &acceptPayload, sizeof(acceptPayload),
                             accountId, steamID64, true, generation))
        return false;

    GSClientApprovePayload approvePayload;
    approvePayload.steamID = steamID;
    approvePayload.ownerSteamID = steamID;
    if (!QueueCallbackLocked(state, kCallbackGSClientApprove, &approvePayload, sizeof(approvePayload),
                             accountId, steamID64, true, generation))
    {
        // The preflight above makes this impossible unless a future payload
        // rule changes. Keep rollback explicit so the pair remains atomic.
        if (!state.callbacks.empty())
            state.callbacks.pop_back();
        return false;
    }
    return true;
}

size_t RemovePendingAuthCallbacksLocked(RuntimeState &state, uint32 accountId, uint64 generation)
{
    size_t removed = 0;
    std::deque<QueuedCallback>::iterator it = state.callbacks.begin();
    if (state.callbackInFlight && it != state.callbacks.end())
        ++it; // The caller owns the front payload pointer until Steam_FreeLastCallback.

    while (it != state.callbacks.end())
    {
        if (it->authCallback && it->accountId == accountId && (generation == 0 || it->generation == generation))
        {
            it = state.callbacks.erase(it);
            ++removed;
        }
        else
        {
            ++it;
        }
    }
    return removed;
}

bool GetNextCallback(HSteamPipe pipe, CallbackMsg_t *msg)
{
    RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!msg || pipe != kPipe || state.callbackInFlight || state.callbacks.empty())
        return false;

    QueuedCallback &item = state.callbacks.front();
    msg->m_hSteamUser = item.user;
    msg->m_iCallback = item.callback;
    msg->m_pubParam = item.payload.empty() ? NULL : item.payload.data();
    msg->m_cubParam = static_cast<int>(item.payload.size());
    state.currentCallbackUser = item.user;
    state.callbackInFlight = true;
    Log("Callback", "Steam_BGetCallback id=%d size=%d", msg->m_iCallback, msg->m_cubParam);
    return true;
}

void FreeLastCallback(HSteamPipe pipe)
{
    RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (pipe == kPipe && state.callbackInFlight && !state.callbacks.empty())
    {
        const QueuedCallback &item = state.callbacks.front();
        ApplyAuthCallbackLifecycleLocked(state, item);
        state.callbacks.pop_front();
    }
    state.callbackInFlight = false;
}

} // namespace legacy
} // namespace revive
