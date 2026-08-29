#pragma once

#include "LegacySteamRuntime.h"

namespace revive
{
namespace legacy
{

void QueueCallback(int callback, const void *payload, size_t payloadSize);
void QueueCallbackLocked(RuntimeState &state, int callback, const void *payload, size_t payloadSize,
                         uint32 accountId, uint64 steamID, bool authCallback);
void QueueClientApprove(const CSteamID &steamID);
void QueueSteam2AuthCallbacksLocked(RuntimeState &state, uint32 accountId, const CSteamID &steamID);
size_t RemovePendingAuthCallbacksLocked(RuntimeState &state, uint32 accountId);
bool GetNextCallback(HSteamPipe pipe, CallbackMsg_t *msg);
void FreeLastCallback(HSteamPipe pipe);

} // namespace legacy
} // namespace revive
