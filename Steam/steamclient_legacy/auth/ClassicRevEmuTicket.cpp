#include <cstring>

#include "ClassicRevEmuTicket.h"

namespace revive
{
namespace legacy
{
namespace auth
{

namespace
{

uint32 ReadLE32(const uint8 *p)
{
    return static_cast<uint32>(p[0]) |
           (static_cast<uint32>(p[1]) << 8) |
           (static_cast<uint32>(p[2]) << 16) |
           (static_cast<uint32>(p[3]) << 24);
}

} // namespace

const char *ClassicRevEmuTicketResultString(ClassicRevEmuTicketResult result)
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

ClassicRevEmuTicketResult ValidateClassicRevEmuTicket(const void *ticket, uint32 ticketSize,
                                                       ClassicRevEmuTicketInfo *out)
{
    // Keep the accepted M3.1 behavior: length is checked before the pointer,
    // therefore NULL/0 is a truncated ticket while NULL/152 is a null ticket.
    if (ticketSize != kClassicRevEmuTicketSize)
        return kClassicRevEmuTicketInvalidLength;
    if (!ticket)
        return kClassicRevEmuTicketNull;

    const uint8 *p = static_cast<const uint8 *>(ticket);
    const uint32 header = ReadLE32(p + 0x00);
    const uint32 hash = ReadLE32(p + 0x04);
    const uint32 magic = ReadLE32(p + 0x08);
    const uint32 reserved = ReadLE32(p + 0x0c);
    const uint32 steamIDLow = ReadLE32(p + 0x10);
    const uint32 steamIDHigh = ReadLE32(p + 0x14);

    if (header != kClassicRevEmuHeader)
        return kClassicRevEmuTicketInvalidHeader;
    if (magic != kClassicRevEmuMagic)
        return kClassicRevEmuTicketInvalidMagic;
    if (reserved != 0)
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

bool ParseClassicRevEmuTicket(const void *ticket, uint32 ticketSize, ClassicRevEmuTicketInfo *out)
{
    return ValidateClassicRevEmuTicket(ticket, ticketSize, out) == kClassicRevEmuTicketValid;
}

} // namespace auth
} // namespace legacy
} // namespace revive
