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

void QueueCallbackLocked(RuntimeState &state, int callback, const void *payload, size_t payloadSize,
                         uint32 accountId, uint64 steamID, bool authCallback, uint64 generation)
{
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
}

void QueueCallback(int callback, const void *payload, size_t payloadSize)
{
    RuntimeState &state = Runtime();
    std::lock_guard<std::mutex> lock(state.mutex);
    QueueCallbackLocked(state, callback, payload, payloadSize, 0, 0, false, 0);
}

void QueueClientApprove(const CSteamID &steamID)
{
    GSClientApprovePayload payload;
    payload.steamID = steamID;
    payload.ownerSteamID = steamID;
    QueueCallback(kCallbackGSClientApprove, &payload, sizeof(payload));
}

void QueueSteam2AuthCallbacksLocked(RuntimeState &state, uint32 accountId, const CSteamID &steamID, uint64 generation)
{
    const uint64 steamID64 = steamID.ConvertToUint64();

    GSClientSteam2AcceptPayload acceptPayload;
    acceptPayload.userID = accountId;
    acceptPayload.steamID = steamID64;
    QueueCallbackLocked(state, kCallbackGSClientSteam2Accept, &acceptPayload, sizeof(acceptPayload),
                        accountId, steamID64, true, generation);

    GSClientApprovePayload approvePayload;
    approvePayload.steamID = steamID;
    approvePayload.ownerSteamID = steamID;
    QueueCallbackLocked(state, kCallbackGSClientApprove, &approvePayload, sizeof(approvePayload),
                        accountId, steamID64, true, generation);
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
