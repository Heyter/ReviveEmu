#pragma once

#include "LegacySteamRuntime.h"

namespace revive
{
namespace legacy
{

bool QueueCallback(int callback, const void *payload, size_t payloadSize);
bool QueueCallbackLocked(RuntimeState &state, int callback, const void *payload, size_t payloadSize,
                         uint32 accountId, uint64 steamID, bool authCallback, uint64 generation);
bool QueueClientApprove(const CSteamID &steamID);
bool QueueSteam2AuthCallbacksLocked(RuntimeState &state, uint32 accountId, const CSteamID &steamID, uint64 generation);
size_t RemovePendingAuthCallbacksLocked(RuntimeState &state, uint32 accountId, uint64 generation = 0);
bool GetNextCallback(HSteamPipe pipe, CallbackMsg_t *msg);
void FreeLastCallback(HSteamPipe pipe);

} // namespace legacy
} // namespace revive
