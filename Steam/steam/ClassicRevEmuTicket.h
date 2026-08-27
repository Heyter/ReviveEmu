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

inline bool ParseClassicRevEmuTicket(const void *ticket,
                                     uint32 ticketSize,
                                     ClassicRevEmuTicketInfo *out)
{
    if (!ticket || ticketSize != kClassicRevEmuTicketSize)
        return false;

    const uint8 *p = static_cast<const uint8 *>(ticket);
    const uint32 header = ReadLE32(p + 0x00);
    const uint32 hash = ReadLE32(p + 0x04);
    const uint32 magic = ReadLE32(p + 0x08);
    const uint32 magic2 = ReadLE32(p + 0x0c);
    const uint32 steamIDLow = ReadLE32(p + 0x10);
    const uint32 steamIDHigh = ReadLE32(p + 0x14);

    if (header != kClassicRevEmuHeader ||
        magic != kClassicRevEmuMagic ||
        magic2 != 0 ||
        steamIDHigh != kClassicRevEmuSteamIDHigh ||
        steamIDLow != hash * 2u)
    {
        return false;
    }

    if (out)
    {
        out->hash = hash;
        out->steamIDLow = steamIDLow;
        out->steamIDHigh = steamIDHigh;
        out->steamID.SetFromUint64((static_cast<uint64>(steamIDHigh) << 32) | steamIDLow);
        std::memcpy(out->hwid, p + 0x18, sizeof(out->hwid));
    }

    return true;
}

} // namespace revive
