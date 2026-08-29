#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../auth/LegacyAuthTicket.h"

namespace
{

using namespace revive::legacy::auth;

void Fail(const char *message)
{
    std::fprintf(stderr, "legacy auth ticket smoke FAIL: %s\n", message ? message : "unknown error");
    std::exit(1);
}

void Check(bool condition, const char *message)
{
    if (!condition)
        Fail(message);
}

void WriteLE32(uint8 *p, uint32 value)
{
    p[0] = static_cast<uint8>(value);
    p[1] = static_cast<uint8>(value >> 8);
    p[2] = static_cast<uint8>(value >> 16);
    p[3] = static_cast<uint8>(value >> 24);
}

void MakeClassicTicket(uint8 (&ticket)[kClassicRevEmuTicketSize], uint32 hash, uint8 hwidSeed)
{
    std::memset(ticket, 0, sizeof(ticket));
    WriteLE32(ticket + 0x00, kClassicRevEmuHeader);
    WriteLE32(ticket + 0x04, hash);
    WriteLE32(ticket + 0x08, kClassicRevEmuMagic);
    WriteLE32(ticket + 0x0c, 0);
    WriteLE32(ticket + 0x10, hash * 2u);
    WriteLE32(ticket + 0x14, kClassicRevEmuSteamIDHigh);
    for (size_t i = 0; i < 128; ++i)
        ticket[0x18 + i] = static_cast<uint8>(hwidSeed + static_cast<uint8>(i));
}

} // namespace

int main()
{
    const uint32 hash = 1471518829u;
    uint8 valid[kClassicRevEmuTicketSize];
    MakeClassicTicket(valid, hash, 0x21);

    Check(DetectAuthTicketType(NULL, 0) == kAuthTicketUnknown, "NULL/0 detection mismatch");
    Check(ParseAndValidateAuthTicket(NULL, 0, NULL) == kAuthTicketInvalidLength,
          "NULL/0 must preserve invalid_length semantics");
    Check(ParseAndValidateAuthTicket(NULL, kClassicRevEmuTicketSize, NULL) == kAuthTicketNull,
          "NULL/152 must be null_ticket");
    Check(ParseAndValidateAuthTicket(valid, kClassicRevEmuTicketSize - 1, NULL) == kAuthTicketInvalidLength,
          "truncated ticket must be rejected");

    uint8 unknown[kClassicRevEmuTicketSize];
    std::memset(unknown, 0x5a, sizeof(unknown));
    Check(DetectAuthTicketType(unknown, sizeof(unknown)) == kAuthTicketUnknown,
          "unknown ticket incorrectly detected as ClassicRevEmu");
    Check(ParseAndValidateAuthTicket(unknown, sizeof(unknown), NULL) == kAuthTicketUnsupportedFormat,
          "unknown 152-byte ticket must be unsupported_format");

    uint8 corrupted[kClassicRevEmuTicketSize];
    std::memcpy(corrupted, valid, sizeof(corrupted));
    corrupted[0] ^= 1u;
    Check(DetectAuthTicketType(corrupted, sizeof(corrupted)) == kAuthTicketClassicRevEmu,
          "ClassicRevEmu detection must survive one damaged discriminator");
    Check(ParseAndValidateAuthTicket(corrupted, sizeof(corrupted), NULL) == kAuthTicketInvalidHeader,
          "corrupt header reason mismatch");

    std::memcpy(corrupted, valid, sizeof(corrupted));
    corrupted[8] ^= 1u;
    Check(ParseAndValidateAuthTicket(corrupted, sizeof(corrupted), NULL) == kAuthTicketInvalidMagic,
          "corrupt magic reason mismatch");

    AuthTicketIdentity identity;
    Check(ParseAndValidateAuthTicket(valid, sizeof(valid), &identity) == kAuthTicketValid,
          "valid ClassicRevEmu ticket rejected");
    Check(identity.type == kAuthTicketClassicRevEmu, "valid ticket type mismatch");
    Check(identity.classicHash == hash, "ClassicRevEmu hash mismatch");
    Check(identity.classicSteamIDLow == hash * 2u, "ClassicRevEmu SteamID low mismatch");
    Check(identity.classicSteamIDHigh == kClassicRevEmuSteamIDHigh, "ClassicRevEmu SteamID high mismatch");
    Check(identity.steamID.GetAccountID() == hash * 2u, "ticket-derived SteamID account mismatch");
    Check(identity.fingerprint != 0, "valid ticket fingerprint must be non-zero");
    Check(identity.fingerprint == AuthTicketFingerprint(valid, sizeof(valid)), "fingerprint must be deterministic");

    uint8 sameIdentityDifferentTicket[kClassicRevEmuTicketSize];
    MakeClassicTicket(sameIdentityDifferentTicket, hash, 0x31);
    AuthTicketIdentity secondIdentity;
    Check(ParseAndValidateAuthTicket(sameIdentityDifferentTicket, sizeof(sameIdentityDifferentTicket), &secondIdentity) == kAuthTicketValid,
          "second valid ClassicRevEmu ticket rejected");
    Check(secondIdentity.steamID.ConvertToUint64() == identity.steamID.ConvertToUint64(),
          "same ClassicRevEmu hash must produce same SteamID");
    Check(secondIdentity.fingerprint != identity.fingerprint,
          "different ticket bytes must produce a different replay fingerprint");

    std::puts("[PASS] M3.6 auth ticket detection/parsing/validation smoke");
    return 0;
}
