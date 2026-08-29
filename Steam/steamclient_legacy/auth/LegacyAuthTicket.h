#pragma once

#include "ClassicRevEmuTicket.h"

namespace revive
{
namespace legacy
{
namespace auth
{

#if defined(__GNUC__)
#define REVIVE_AUTH_TICKET_INTERNAL __attribute__((visibility("hidden")))
#else
#define REVIVE_AUTH_TICKET_INTERNAL
#endif

enum AuthTicketType
{
    kAuthTicketUnknown = 0,
    kAuthTicketClassicRevEmu
};

enum AuthTicketResult
{
    kAuthTicketValid = 0,
    kAuthTicketInvalidLength,
    kAuthTicketNull,
    kAuthTicketUnsupportedFormat,
    kAuthTicketInvalidHeader,
    kAuthTicketInvalidMagic,
    kAuthTicketInvalidReserved,
    kAuthTicketInvalidSteamIDHigh,
    kAuthTicketInvalidIdentityRelation,
    kAuthTicketInvalidIdentity
};

struct AuthTicketIdentity
{
    AuthTicketIdentity();

    AuthTicketType type;
    CSteamID steamID;
    uint64 fingerprint;
    uint32 classicHash;
    uint32 classicSteamIDLow;
    uint32 classicSteamIDHigh;
};

REVIVE_AUTH_TICKET_INTERNAL AuthTicketType DetectAuthTicketType(const void *ticket, uint32 ticketSize);
REVIVE_AUTH_TICKET_INTERNAL AuthTicketResult ParseAndValidateAuthTicket(const void *ticket, uint32 ticketSize, AuthTicketIdentity *out);
REVIVE_AUTH_TICKET_INTERNAL const char *AuthTicketTypeString(AuthTicketType type);
REVIVE_AUTH_TICKET_INTERNAL const char *AuthTicketResultString(AuthTicketResult result);
REVIVE_AUTH_TICKET_INTERNAL uint64 AuthTicketFingerprint(const void *ticket, uint32 ticketSize);

#undef REVIVE_AUTH_TICKET_INTERNAL

} // namespace auth
} // namespace legacy
} // namespace revive
