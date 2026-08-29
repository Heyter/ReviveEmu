#include <cstring>

#include "LegacyAuthTicket.h"

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

AuthTicketResult ConvertClassicResult(ClassicRevEmuTicketResult result)
{
    switch (result)
    {
        case kClassicRevEmuTicketValid: return kAuthTicketValid;
        case kClassicRevEmuTicketInvalidLength: return kAuthTicketInvalidLength;
        case kClassicRevEmuTicketNull: return kAuthTicketNull;
        case kClassicRevEmuTicketInvalidHeader: return kAuthTicketInvalidHeader;
        case kClassicRevEmuTicketInvalidMagic: return kAuthTicketInvalidMagic;
        case kClassicRevEmuTicketInvalidReserved: return kAuthTicketInvalidReserved;
        case kClassicRevEmuTicketInvalidSteamIDHigh: return kAuthTicketInvalidSteamIDHigh;
        case kClassicRevEmuTicketInvalidIdentityRelation: return kAuthTicketInvalidIdentityRelation;
        case kClassicRevEmuTicketInvalidIdentity: return kAuthTicketInvalidIdentity;
        default: return kAuthTicketUnsupportedFormat;
    }
}

} // namespace

AuthTicketIdentity::AuthTicketIdentity()
    : type(kAuthTicketUnknown), steamID(), fingerprint(0), classicHash(0),
      classicSteamIDLow(0), classicSteamIDHigh(0)
{
}

AuthTicketType DetectAuthTicketType(const void *ticket, uint32 ticketSize)
{
    if (!ticket || ticketSize != kClassicRevEmuTicketSize)
        return kAuthTicketUnknown;

    const uint8 *p = static_cast<const uint8 *>(ticket);
    const uint32 header = ReadLE32(p + 0x00);
    const uint32 magic = ReadLE32(p + 0x08);

    // A single intact discriminator is enough to route malformed ClassicRevEmu
    // tickets to its strict validator and retain a precise reject reason.
    if (header == kClassicRevEmuHeader || magic == kClassicRevEmuMagic)
        return kAuthTicketClassicRevEmu;

    return kAuthTicketUnknown;
}

uint64 AuthTicketFingerprint(const void *ticket, uint32 ticketSize)
{
    if (!ticket || ticketSize == 0)
        return 0;

    // FNV-1a is only a stable local replay discriminator. It is not used as an
    // authentication primitive and never replaces the SteamID carried by the ticket.
    const uint8 *p = static_cast<const uint8 *>(ticket);
    uint64 hash = static_cast<uint64>(14695981039346656037ULL);
    for (uint32 i = 0; i < ticketSize; ++i)
    {
        hash ^= static_cast<uint64>(p[i]);
        hash *= static_cast<uint64>(1099511628211ULL);
    }
    return hash;
}

AuthTicketResult ParseAndValidateAuthTicket(const void *ticket, uint32 ticketSize, AuthTicketIdentity *out)
{
    if (ticketSize != kClassicRevEmuTicketSize)
        return kAuthTicketInvalidLength;
    if (!ticket)
        return kAuthTicketNull;

    const AuthTicketType type = DetectAuthTicketType(ticket, ticketSize);
    if (type == kAuthTicketUnknown)
        return kAuthTicketUnsupportedFormat;

    ClassicRevEmuTicketInfo classicInfo;
    const ClassicRevEmuTicketResult classicResult =
        ValidateClassicRevEmuTicket(ticket, ticketSize, &classicInfo);
    const AuthTicketResult result = ConvertClassicResult(classicResult);
    if (result != kAuthTicketValid)
        return result;

    if (out)
    {
        out->type = kAuthTicketClassicRevEmu;
        out->steamID = classicInfo.steamID;
        out->fingerprint = AuthTicketFingerprint(ticket, ticketSize);
        out->classicHash = classicInfo.hash;
        out->classicSteamIDLow = classicInfo.steamIDLow;
        out->classicSteamIDHigh = classicInfo.steamIDHigh;
    }

    return kAuthTicketValid;
}

const char *AuthTicketTypeString(AuthTicketType type)
{
    switch (type)
    {
        case kAuthTicketClassicRevEmu: return "ClassicRevEmu";
        case kAuthTicketUnknown: return "Unknown";
        default: return "Unknown";
    }
}

const char *AuthTicketResultString(AuthTicketResult result)
{
    switch (result)
    {
        case kAuthTicketValid: return "valid";
        case kAuthTicketInvalidLength: return "invalid_length";
        case kAuthTicketNull: return "null_ticket";
        case kAuthTicketUnsupportedFormat: return "unsupported_format";
        case kAuthTicketInvalidHeader: return "invalid_header";
        case kAuthTicketInvalidMagic: return "invalid_magic";
        case kAuthTicketInvalidReserved: return "invalid_reserved";
        case kAuthTicketInvalidSteamIDHigh: return "invalid_steamid_high";
        case kAuthTicketInvalidIdentityRelation: return "invalid_identity_relation";
        case kAuthTicketInvalidIdentity: return "invalid_identity";
        default: return "unknown";
    }
}

} // namespace auth
} // namespace legacy
} // namespace revive
