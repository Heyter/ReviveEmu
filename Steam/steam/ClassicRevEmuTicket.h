#pragma once

#include <cstring>
#include "RevCommon.h"
#include "Steam3ID.h"

namespace revive
{

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

inline const char *ClassicRevEmuTicketResultString(ClassicRevEmuTicketResult result)
{
    switch (result)
    {
        case kClassicRevEmuTicketValid: return "valid";
        case kClassicRevEmuTicketInvalidLength: return "invalid_length";
        case kClassicRevEmuTicketNull: return "null_ticket";
        case kClassicRevEmuTicketInvalidHeader: return "invalid_header";
        case kClassicRevEmuTicketInvalidMagic: return "invalid_magic";
        case kClassicRevEmuTicketInvalidReserved: return "invalid_reserved";
        case kClassicRevEmuTicketInvalidSteamIDHigh: return "invalid_steamid_high";
        case kClassicRevEmuTicketInvalidIdentityRelation: return "invalid_identity_relation";
        case kClassicRevEmuTicketInvalidIdentity: return "invalid_identity";
        default: return "unknown";
    }
}

struct ClassicRevEmuTicketInfo
{
    uint32 hash;
    uint32 steamIDLow;
    uint32 steamIDHigh;
    CSteamID steamID;
    uint8 hwid[128];
};

inline uint32 ReadLE32(const uint8 *p)
{
    return static_cast<uint32>(p[0]) |
           (static_cast<uint32>(p[1]) << 8) |
           (static_cast<uint32>(p[2]) << 16) |
           (static_cast<uint32>(p[3]) << 24);
}

inline ClassicRevEmuTicketResult ValidateClassicRevEmuTicket(const void *ticket,
                                                               uint32 ticketSize,
                                                               ClassicRevEmuTicketInfo *out)
{
    // Length is checked before the pointer so NULL/0 produces the same useful
    // reject reason as any other truncated ticket.
    if (ticketSize != kClassicRevEmuTicketSize)
        return kClassicRevEmuTicketInvalidLength;
    if (!ticket)
        return kClassicRevEmuTicketNull;

    const uint8 *p = static_cast<const uint8 *>(ticket);
    const uint32 header = ReadLE32(p + 0x00);
    const uint32 hash = ReadLE32(p + 0x04);
    const uint32 magic = ReadLE32(p + 0x08);
    const uint32 magic2 = ReadLE32(p + 0x0c);
    const uint32 steamIDLow = ReadLE32(p + 0x10);
    const uint32 steamIDHigh = ReadLE32(p + 0x14);

    if (header != kClassicRevEmuHeader)
        return kClassicRevEmuTicketInvalidHeader;
    if (magic != kClassicRevEmuMagic)
        return kClassicRevEmuTicketInvalidMagic;
    if (magic2 != 0)
        return kClassicRevEmuTicketInvalidReserved;
    if (steamIDHigh != kClassicRevEmuSteamIDHigh)
        return kClassicRevEmuTicketInvalidSteamIDHigh;
    if (steamIDLow != hash * 2u)
        return kClassicRevEmuTicketInvalidIdentityRelation;
    if (hash == 0 || steamIDLow == 0)
        return kClassicRevEmuTicketInvalidIdentity;

    CSteamID steamID;
    steamID.SetFromUint64((static_cast<uint64>(steamIDHigh) << 32) | steamIDLow);
    if (!steamID.IsValid())
        return kClassicRevEmuTicketInvalidIdentity;

    if (out)
    {
        out->hash = hash;
        out->steamIDLow = steamIDLow;
        out->steamIDHigh = steamIDHigh;
        out->steamID = steamID;
        std::memcpy(out->hwid, p + 0x18, sizeof(out->hwid));
    }

    return kClassicRevEmuTicketValid;
}

inline bool ParseClassicRevEmuTicket(const void *ticket,
                                     uint32 ticketSize,
                                     ClassicRevEmuTicketInfo *out)
{
    return ValidateClassicRevEmuTicket(ticket, ticketSize, out) == kClassicRevEmuTicketValid;
}

} // namespace revive
