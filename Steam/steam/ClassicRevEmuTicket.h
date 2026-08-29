#pragma once

// Compatibility include for older ReviveEmu source paths. The actual ticket
// format implementation belongs to the M3.6 authentication layer.
#include "../steamclient_legacy/auth/ClassicRevEmuTicket.h"

namespace revive
{
using legacy::auth::ClassicRevEmuTicketInfo;
using legacy::auth::ClassicRevEmuTicketResult;
using legacy::auth::ClassicRevEmuTicketResultString;
using legacy::auth::ValidateClassicRevEmuTicket;
using legacy::auth::ParseClassicRevEmuTicket;
using legacy::auth::kClassicRevEmuHeader;
using legacy::auth::kClassicRevEmuMagic;
using legacy::auth::kClassicRevEmuSteamIDHigh;
using legacy::auth::kClassicRevEmuTicketSize;
using legacy::auth::kClassicRevEmuTicketValid;
using legacy::auth::kClassicRevEmuTicketInvalidLength;
using legacy::auth::kClassicRevEmuTicketNull;
using legacy::auth::kClassicRevEmuTicketInvalidHeader;
using legacy::auth::kClassicRevEmuTicketInvalidMagic;
using legacy::auth::kClassicRevEmuTicketInvalidReserved;
using legacy::auth::kClassicRevEmuTicketInvalidSteamIDHigh;
using legacy::auth::kClassicRevEmuTicketInvalidIdentityRelation;
using legacy::auth::kClassicRevEmuTicketInvalidIdentity;
}
