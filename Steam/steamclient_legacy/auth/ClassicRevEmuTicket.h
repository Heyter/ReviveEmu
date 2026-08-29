#pragma once

#include <cstddef>

#include "../../steam/SteamCommon.h"
#include "../../steam/RevCommon.h"
#include "../../steam/Steam3ID.h"

namespace revive
{
namespace legacy
{
namespace auth
{

#if defined(__GNUC__)
#define REVIVE_AUTH_INTERNAL __attribute__((visibility("hidden")))
#else
#define REVIVE_AUTH_INTERNAL
#endif

static const uint32 kClassicRevEmuTicketSize = 152;
static const uint32 kClassicRevEmuHeader = 0x00000053u;
static const uint32 kClassicRevEmuMagic = 0x00726576u; // "rev\0"
static const uint32 kClassicRevEmuSteamIDHigh = 0x01100001u;

enum ClassicRevEmuTicketResult
{
    kClassicRevEmuTicketValid = 0,
    kClassicRevEmuTicketInvalidLength,
    kClassicRevEmuTicketNull,
    kClassicRevEmuTicketInvalidHeader,
    kClassicRevEmuTicketInvalidMagic,
    kClassicRevEmuTicketInvalidReserved,
    kClassicRevEmuTicketInvalidSteamIDHigh,
    kClassicRevEmuTicketInvalidIdentityRelation,
    kClassicRevEmuTicketInvalidIdentity
};

struct ClassicRevEmuTicketInfo
{
    uint32 hash;
    uint32 steamIDLow;
    uint32 steamIDHigh;
    CSteamID steamID;
    uint8 hwid[128];
};

REVIVE_AUTH_INTERNAL const char *ClassicRevEmuTicketResultString(ClassicRevEmuTicketResult result);
REVIVE_AUTH_INTERNAL ClassicRevEmuTicketResult ValidateClassicRevEmuTicket(const void *ticket, uint32 ticketSize,
                                                       ClassicRevEmuTicketInfo *out);
REVIVE_AUTH_INTERNAL bool ParseClassicRevEmuTicket(const void *ticket, uint32 ticketSize, ClassicRevEmuTicketInfo *out);

#undef REVIVE_AUTH_INTERNAL

} // namespace auth
} // namespace legacy
} // namespace revive
