#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>

namespace
{

typedef int32_t HSteamPipe;
typedef int32_t HSteamUser;

enum EAccountType
{
    k_EAccountTypeInvalid = 0,
    k_EAccountTypeIndividual = 1,
    k_EAccountTypeGameServer = 3,
    k_EAccountTypeAnonGameServer = 4
};

enum EUniverse
{
    k_EUniverseInvalid = 0,
    k_EUniversePublic = 1
};

struct CSteamID
{
    uint64_t value;
};

struct CallbackMsg_t
{
    HSteamUser m_hSteamUser;
    int m_iCallback;
    uint8_t *m_pubParam;
    int m_cubParam;
};

static_assert(sizeof(CSteamID) == 8, "legacy CSteamID must be 8 bytes");
static_assert(sizeof(CallbackMsg_t) == (sizeof(void *) == 4 ? 16u : 24u),
              "legacy CallbackMsg_t layout mismatch");
static_assert(sizeof(void *) != 4 || offsetof(CallbackMsg_t, m_pubParam) == 8,
              "legacy x86 CallbackMsg_t pointer offset mismatch");
static_assert(sizeof(void *) != 4 || offsetof(CallbackMsg_t, m_cubParam) == 12,
              "legacy x86 CallbackMsg_t size offset mismatch");

#pragma pack(push, 8)
struct GSClientApprovePayload
{
    uint64_t steamID;
    uint64_t ownerSteamID;
};

struct GSClientSteam2AcceptPayload
{
    uint32_t userID;
    uint64_t steamID;
};
#pragma pack(pop)

static_assert(sizeof(GSClientApprovePayload) == 16, "callback 201 ABI must be 16 bytes");
static_assert(sizeof(GSClientSteam2AcceptPayload) == (sizeof(void *) == 4 ? 12u : 16u),
              "callback 205 ABI layout mismatch");

void Fail(const char *message)
{
    std::fprintf(stderr, "M2.3 ABI smoke FAIL: %s\n", message ? message : "unknown error");
    std::exit(1);
}

void Check(bool condition, const char *message)
{
    if (!condition)
        Fail(message);
}

template <typename T>
T Sym(void *library, const char *name)
{
    dlerror();
    void *symbol = dlsym(library, name);
    const char *error = dlerror();
    if (error || !symbol)
    {
        std::fprintf(stderr, "M2.3 ABI smoke FAIL: missing symbol %s (%s)\n",
                     name, error ? error : "null");
        std::exit(1);
    }
    return reinterpret_cast<T>(symbol);
}

void **VTable(void *object)
{
    Check(object != NULL, "null C++ interface object");
    void **table = *reinterpret_cast<void ***>(object);
    Check(table != NULL, "null C++ interface vtable");
    return table;
}

template <typename T>
T VSlot(void *object, int slot)
{
    void *entry = VTable(object)[slot];
    Check(entry != NULL, "null C++ vtable slot");
    return reinterpret_cast<T>(entry);
}

uint32_t ExpectedServerAccount(uint32_t ip, uint16_t port)
{
    uint32_t account = ip ^ (static_cast<uint32_t>(port) << 16) ^ 0x52455649u;
    return account ? account : 0x539u;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2)
        Fail("usage: steamclient_abi_smoke <libsteamclient.so>");

    void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library)
        Fail(dlerror());

    typedef void *(*CreateInterfaceFn)(const char *, int *);
    typedef bool (*SteamBGetCallbackFn)(HSteamPipe, CallbackMsg_t *);
    typedef void (*SteamFreeLastCallbackFn)(HSteamPipe);
    typedef void (*SteamRunCallbacksFn)(HSteamPipe, bool);
    typedef bool (*SteamGetAPICallResultFn)(HSteamPipe, uint64_t, void *, int, int, bool *);
    typedef HSteamUser (*SteamCreateLocalUserFn)(HSteamPipe *, EAccountType);
    typedef int (*SteamGSGetConnectTokenFn)(HSteamUser, HSteamPipe, void *, int);
    typedef void *(*SteamGetGSHandleFn)(HSteamUser, HSteamPipe);
    typedef void (*SteamGSLogOnFn)(void *);
    typedef void (*SteamGSLogOffFn)(void *);
    typedef bool (*SteamGSBLoggedOnFn)(void *);
    typedef bool (*SteamGSBSecureFn)(void *);
    typedef bool (*SteamGSGetKeyFn)(void *, void *, uint32_t *, uint32_t);
    typedef bool (*SteamGSSendSteam2Fn)(void *, uint32_t, const void *, uint32_t, uint32_t, uint16_t, const void *, uint32_t);
    typedef bool (*SteamGSSetServerTypeFn)(void *, int32_t, uint32_t, uint32_t, uint32_t, const char *, const char *);
    typedef uint64_t (*SteamGSGetSteamIDFn)(void *);
    typedef int (*SteamInitiateGameConnectionFn)(HSteamUser, HSteamPipe, void *, int, uint64_t, int, uint32_t, uint16_t, bool);
    typedef void (*SteamTerminateGameConnectionFn)(HSteamUser, HSteamPipe, uint32_t, uint16_t);
    typedef const char *(*BuildMarkerFn)();

    CreateInterfaceFn createInterface = Sym<CreateInterfaceFn>(library, "CreateInterface");
    SteamBGetCallbackFn getCallback = Sym<SteamBGetCallbackFn>(library, "Steam_BGetCallback");
    SteamFreeLastCallbackFn freeCallback = Sym<SteamFreeLastCallbackFn>(library, "Steam_FreeLastCallback");
    SteamRunCallbacksFn runCallbacks = Sym<SteamRunCallbacksFn>(library, "Steam_RunCallbacks");
    SteamGetAPICallResultFn getAPICallResult = Sym<SteamGetAPICallResultFn>(library, "Steam_GetAPICallResult");
    SteamCreateLocalUserFn createLocalUser = Sym<SteamCreateLocalUserFn>(library, "Steam_CreateLocalUser");
    SteamGSGetConnectTokenFn gsGetConnectToken = Sym<SteamGSGetConnectTokenFn>(library, "Steam_GSGetSteamGameConnectToken");
    SteamGetGSHandleFn getGSHandle = Sym<SteamGetGSHandleFn>(library, "Steam_GetGSHandle");
    SteamGSLogOnFn gsLogOn = Sym<SteamGSLogOnFn>(library, "Steam_GSLogOn");
    SteamGSLogOffFn gsLogOff = Sym<SteamGSLogOffFn>(library, "Steam_GSLogOff");
    SteamGSBLoggedOnFn gsBLoggedOn = Sym<SteamGSBLoggedOnFn>(library, "Steam_GSBLoggedOn");
    SteamGSBSecureFn gsBSecure = Sym<SteamGSBSecureFn>(library, "Steam_GSBSecure");
    SteamGSGetKeyFn gsGetKey = Sym<SteamGSGetKeyFn>(library, "Steam_GSGetSteam2GetEncryptionKeyToSendToNewClient");
    SteamGSSendSteam2Fn gsSendSteam2 = Sym<SteamGSSendSteam2Fn>(library, "Steam_GSSendSteam2UserConnect");
    SteamGSSetServerTypeFn gsSetServerType = Sym<SteamGSSetServerTypeFn>(library, "Steam_GSSetServerType");
    SteamGSGetSteamIDFn gsGetSteamID = Sym<SteamGSGetSteamIDFn>(library, "Steam_GSGetSteamID");
    SteamInitiateGameConnectionFn initiate = Sym<SteamInitiateGameConnectionFn>(library, "Steam_InitiateGameConnection");
    SteamTerminateGameConnectionFn terminate = Sym<SteamTerminateGameConnectionFn>(library, "Steam_TerminateGameConnection");
    BuildMarkerFn buildMarker = Sym<BuildMarkerFn>(library, "REVive_LegacySteamClient_BuildMarker");

    Check(std::strcmp(buildMarker(), "REVive legacy SteamClient006 backend M2.3-dev.2") == 0,
          "unexpected build marker");

    int rc = -1;
    void *client = createInterface("SteamClient006", &rc);
    Check(client && rc == 0, "SteamClient006 CreateInterface failed");

    typedef HSteamPipe (*ClientCreateSteamPipeVFn)(void *);
    typedef HSteamUser (*ClientCreateLocalUserVFn)(void *, HSteamPipe *);
    typedef void *(*ClientGetGameServerVFn)(void *, HSteamUser, HSteamPipe, const char *);
    typedef void *(*ClientGetUtilsVFn)(void *, HSteamPipe, const char *);
    Check(VSlot<ClientCreateSteamPipeVFn>(client, 0)(client) == 1,
          "SteamClient006 vtable slot 0 failed");

    HSteamPipe cppPipe = 0;
    Check(VSlot<ClientCreateLocalUserVFn>(client, 4)(client, &cppPipe) == 1 && cppPipe == 1,
          "SteamClient006 vtable slot 4 CreateLocalUser failed");

    void *cppGS = VSlot<ClientGetGameServerVFn>(client, 8)(client, 1, 1, "SteamGameServer002");
    Check(cppGS != NULL, "SteamClient006 vtable slot 8 GetISteamGameServer failed");

    void *utils = VSlot<ClientGetUtilsVFn>(client, 12)(client, 1, "SteamUtils001");
    Check(utils != NULL, "SteamClient006 vtable slot 12 GetISteamUtils failed");

    typedef EUniverse (*UtilsGetUniverseVFn)(void *);
    typedef uint32_t (*UtilsGetServerRealTimeVFn)(void *);
    Check(VSlot<UtilsGetUniverseVFn>(utils, 2)(utils) == k_EUniversePublic,
          "SteamUtils001 vtable slot 2 failed");
    Check(VSlot<UtilsGetServerRealTimeVFn>(utils, 3)(utils) != 0,
          "SteamUtils001 vtable slot 3 failed");

    runCallbacks(1, true);

    bool apiCallFailed = false;
    Check(!getAPICallResult(1, 0, NULL, 0, 0, &apiCallFailed) && apiCallFailed,
          "flat Steam_GetAPICallResult stub ABI failed");

    HSteamPipe flatPipe = 0;
    HSteamUser flatUser = createLocalUser(&flatPipe, k_EAccountTypeGameServer);
    Check(flatUser == 1 && flatPipe == 1, "flat Steam_CreateLocalUser ABI failed");

    Check(gsGetConnectToken(flatUser, flatPipe, NULL, 0) == 0,
          "flat Steam_GSGetSteamGameConnectToken ABI failed");
    void *flatGS = getGSHandle(flatUser, flatPipe);
    Check(flatGS == cppGS, "flat Steam_GetGSHandle ABI failed");
    Check(!gsBLoggedOn(flatGS), "game server unexpectedly logged on");
    Check(!gsBSecure(flatGS), "legacy server unexpectedly secure");

    gsLogOn(flatGS);
    Check(gsBLoggedOn(flatGS), "flat Steam_GSLogOn/Steam_GSBLoggedOn failed");

    CallbackMsg_t callback = {};
    Check(getCallback(flatPipe, &callback), "two-argument Steam_BGetCallback failed");
    Check(callback.m_iCallback == 101 && callback.m_cubParam == 0,
          "SteamServersConnected callback mismatch");
    freeCallback(flatPipe);

    uint8_t key[256] = {};
    uint32_t keySize = 0;
    Check(gsGetKey(flatGS, key, &keySize, sizeof(key)), "flat encryption-key call failed");
    Check(keySize == 160, "legacy Steam2 encryption key must stay 160 bytes");

    const uint32_t flatIP = 0x0a010203u;
    const uint16_t flatPort = 27016;
    Check(gsSetServerType(flatGS, 240, 0xA5A50001u, flatIP, flatPort, "cstrike", "1.0.0.34"),
          "flat Steam_GSSetServerType failed");
    Check(static_cast<uint32_t>(gsGetSteamID(flatGS)) == ExpectedServerAccount(flatIP, flatPort),
          "flat SetServerType semantic argument order mismatch");

    typedef bool (*GSSetServerType2VFn)(void *, int32_t, uint32_t, uint32_t, uint16_t, uint16_t, uint16_t,
                                        const char *, const char *, bool);
    typedef bool (*GSUpdateStatus2VFn)(void *, int, int, int, const char *, const char *, const char *);
    typedef bool (*GSObsoleteSetStatusVFn)(void *, int32_t, uint32_t, int, int, int, int,
                                           const char *, const char *, const char *, const char *);

    const uint32_t cppIP = 0x01020304u;
    const uint16_t cppPort = 27017;
    Check(VSlot<GSSetServerType2VFn>(cppGS, 15)(cppGS, 240, 0x5A5A0002u, cppIP,
                                                 cppPort, 27018, 27019, "cstrike", "1.0.0.34", false),
          "SteamGameServer002 vtable slot 15 SetServerType2 failed");
    Check(static_cast<uint32_t>(gsGetSteamID(flatGS)) == ExpectedServerAccount(cppIP, cppPort),
          "SteamGameServer002 SetServerType2 semantic argument order mismatch");
    Check(VSlot<GSUpdateStatus2VFn>(cppGS, 16)(cppGS, 3, 32, 1,
                                               "server-name", "spectator-name", "de_dust2"),
          "SteamGameServer002 vtable slot 16 UpdateStatus2 failed");
    Check(VSlot<GSObsoleteSetStatusVFn>(cppGS, 10)(cppGS, 240, 0x11u, 3, 32, 1, cppPort,
                                                   "server-name", "cstrike", "de_dust2", "1.0.0.34"),
          "SteamGameServer002 vtable slot 10 Obsolete_GSSetStatus failed");

    Check(gsSendSteam2(flatGS, 77, NULL, 0, 0x7f000001u, 27015, NULL, 0),
          "flat Steam_GSSendSteam2UserConnect ABI failed");

    callback = CallbackMsg_t();
    Check(getCallback(flatPipe, &callback), "missing callback 205");
    Check(callback.m_iCallback == 205, "first auth callback must be 205");
    Check(callback.m_cubParam == (sizeof(void *) == 4 ? 12 : 16), "callback 205 payload size mismatch");
    freeCallback(flatPipe);

    callback = CallbackMsg_t();
    Check(getCallback(flatPipe, &callback), "missing callback 201");
    Check(callback.m_iCallback == 201 && callback.m_cubParam == 16,
          "callback 201 payload mismatch");
    freeCallback(flatPipe);

    Check(initiate(flatUser, flatPipe, NULL, 0, 0, 240, 0x7f000001u, 27016, false) == 0,
          "Steam_InitiateGameConnection exact ABI failed");
    terminate(flatUser, flatPipe, 0x7f000001u, 27016);

    gsLogOff(flatGS);
    Check(!gsBLoggedOn(flatGS), "flat Steam_GSLogOff failed");

    dlclose(library);
    std::puts("M2.3 ABI smoke PASS");
    return 0;
}
