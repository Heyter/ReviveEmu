#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "../LegacySteamAuth.h"
#include "../LegacySteamCallbacks.h"
#include "../LegacySteamLifecycle.h"
#include "../LegacySteamRuntime.h"
#include "../auth/LegacyAuthTicket.h"

namespace
{

using namespace revive::legacy;

class StartGate
{
public:
    explicit StartGate(size_t participants)
        : participants_(participants), arrived_(0), open_(false)
    {
    }

    void Wait()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        ++arrived_;
        if (arrived_ == participants_)
        {
            open_ = true;
            condition_.notify_all();
            return;
        }
        while (!open_)
            condition_.wait(lock);
    }

private:
    size_t participants_;
    size_t arrived_;
    bool open_;
    std::mutex mutex_;
    std::condition_variable condition_;
};

void Fail(const char *message)
{
    std::fprintf(stderr, "robustness stress FAIL: %s\n", message ? message : "unknown error");
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

void MakeClassicTicket(uint8 (&ticket)[auth::kClassicRevEmuTicketSize], uint32 hash, uint8 hwidSeed)
{
    std::memset(ticket, 0, sizeof(ticket));
    WriteLE32(ticket + 0x00, auth::kClassicRevEmuHeader);
    WriteLE32(ticket + 0x04, hash);
    WriteLE32(ticket + 0x08, auth::kClassicRevEmuMagic);
    WriteLE32(ticket + 0x0c, 0);
    WriteLE32(ticket + 0x10, hash * 2u);
    WriteLE32(ticket + 0x14, auth::kClassicRevEmuSteamIDHigh);
    for (size_t i = 0; i < 128; ++i)
        ticket[0x18 + i] = static_cast<uint8>(hwidSeed + static_cast<uint8>(i));
}

auth::AuthTicketIdentity MakeIdentity(uint32 accountSeed, uint64 fingerprint)
{
    auth::AuthTicketIdentity identity;
    identity.type = auth::kAuthTicketClassicRevEmu;
    identity.steamID = CSteamID(accountSeed, 1, k_EUniversePublic, k_EAccountTypeIndividual);
    identity.fingerprint = fingerprint;
    return identity;
}

QueuedCallback AuthCallback(int callback, uint32 accountId, uint64 steamID, uint64 generation)
{
    QueuedCallback item;
    item.user = kUser;
    item.callback = callback;
    item.accountId = accountId;
    item.steamID = steamID;
    item.authCallback = true;
    item.generation = generation;
    return item;
}

void ApplyAuthPairLocked(RuntimeState &state, uint32 accountId, const CSteamID &steamID, uint64 generation)
{
    const uint64 steamID64 = steamID.ConvertToUint64();
    ApplyAuthCallbackLifecycleLocked(state,
        AuthCallback(kCallbackGSClientSteam2Accept, accountId, steamID64, generation));
    ApplyAuthCallbackLifecycleLocked(state,
        AuthCallback(kCallbackGSClientApprove, accountId, steamID64, generation));
}

uint32 NextRandom(uint32 &state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

void CheckClean(const RuntimeState &state, const char *message)
{
    Check(state.steam2Users.empty(), message);
    Check(state.callbacks.empty(), "callback state leaked");
    Check(!state.callbackInFlight, "callback in-flight flag leaked");
}

} // namespace

int main()
{
#if !defined(_WIN32)
    setenv("REVIVE_STEAMCLIENT_LOG", "/dev/null", 1);
#endif

    // Deterministic parser abuse corpus. Non-152 lengths must be rejected
    // before dereferencing payload data, including absurd advertised lengths.
    {
        uint8 byte = 0x5a;
        Check(auth::ParseAndValidateAuthTicket(&byte, 0xffffffffu, NULL) == auth::kAuthTicketInvalidLength,
              "huge advertised ticket length was not rejected before dereference");
        Check(auth::ParseAndValidateAuthTicket(NULL, auth::kClassicRevEmuTicketSize, NULL) == auth::kAuthTicketNull,
              "NULL exact-size ticket mismatch");

        uint32 randomState = 0x39c0ffeeu;
        std::vector<uint8> bytes(512);
        size_t rejected = 0;
        for (size_t iteration = 0; iteration < 10000; ++iteration)
        {
            for (size_t i = 0; i < bytes.size(); ++i)
                bytes[i] = static_cast<uint8>(NextRandom(randomState));

            uint32 size = NextRandom(randomState) % 513u;
            if (size == auth::kClassicRevEmuTicketSize)
            {
                // Force unknown/corrupt discriminators so the random corpus can
                // never accidentally become a valid authentication ticket.
                bytes[0] ^= 0x80u;
                bytes[8] ^= 0x40u;
            }
            const auth::AuthTicketResult result =
                auth::ParseAndValidateAuthTicket(bytes.data(), size, NULL);
            Check(result != auth::kAuthTicketValid, "random ticket corpus produced an accepted identity");
            ++rejected;
        }
        Check(rejected == 10000, "random malformed ticket corpus did not complete");
    }
    std::puts("[PASS] malformed/random/oversized ticket corpus: 10000 rejects");

    // Confirm a valid ClassicRevEmu ticket still survives the hardening gate.
    {
        uint8 valid[auth::kClassicRevEmuTicketSize];
        MakeClassicTicket(valid, 1471518829u, 0x41);
        auth::AuthTicketIdentity identity;
        Check(auth::ParseAndValidateAuthTicket(valid, sizeof(valid), &identity) == auth::kAuthTicketValid,
              "valid ClassicRevEmu regression failed");
        Check(identity.steamID.GetAccountID() == 1471518829u * 2u,
              "valid ticket identity changed under robustness hardening");
    }
    std::puts("[PASS] valid ClassicRevEmu ticket regression");

    // Session reservations are bounded. Existing accounts remain addressable
    // even at the ceiling, while a new account is rejected without generation.
    {
        RuntimeState state;
        std::vector<uint32> accounts;
        for (size_t i = 0; i < kMaxSteam2Sessions; ++i)
        {
            const uint32 accountId = static_cast<uint32>(10000 + i);
            Steam2AuthAttempt attempt = BeginSteam2AuthLocked(state, accountId);
            Check(attempt.createdReservation && !attempt.capacityExceeded && attempt.generation != 0,
                  "session capacity rejected below limit");
            accounts.push_back(accountId);
        }
        Check(state.steam2Users.size() == kMaxSteam2Sessions, "session table did not reach exact limit");

        Steam2AuthAttempt existing = BeginSteam2AuthLocked(state, accounts[0]);
        Check(!existing.createdReservation && !existing.capacityExceeded && existing.generation != 0,
              "existing account was blocked at session capacity");

        Steam2AuthAttempt overflow = BeginSteam2AuthLocked(state, 999999u);
        Check(overflow.capacityExceeded && !overflow.createdReservation && overflow.generation == 0,
              "new account was not rejected at session capacity");
        Check(state.steam2Users.size() == kMaxSteam2Sessions, "session overflow changed table size");

        for (size_t i = 0; i < accounts.size(); ++i)
            Check(RemoveSteam2UserLocked(state, accounts[i], NULL, "robustness_capacity_cleanup") == 1,
                  "session capacity cleanup failed");
        CheckClean(state, "session capacity test leaked state");
    }
    std::printf("[PASS] bounded Steam2 session reservations: %u\n", static_cast<unsigned>(kMaxSteam2Sessions));

    // Callback memory is strictly bounded and auth pairs are all-or-nothing.
    {
        RuntimeState state;
        for (size_t i = 0; i < kMaxQueuedCallbacks; ++i)
            Check(QueueCallbackLocked(state, 900, NULL, 0, 0, 0, false, 0),
                  "callback queue rejected below limit");
        Check(state.callbacks.size() == kMaxQueuedCallbacks, "callback queue did not reach exact limit");
        Check(!QueueCallbackLocked(state, 901, NULL, 0, 0, 0, false, 0),
              "callback queue accepted beyond limit");
        Check(state.callbacks.size() == kMaxQueuedCallbacks, "callback queue grew beyond limit");

        uint8 oversizedPayload[kMaxCallbackPayloadBytes + 1];
        Check(!QueueCallbackLocked(state, 902, oversizedPayload, sizeof(oversizedPayload), 0, 0, false, 0),
              "oversized callback payload was accepted");
        Check(!QueueCallbackLocked(state, 903, NULL, 1, 0, 0, false, 0),
              "NULL callback payload with non-zero size was accepted");

        state.callbacks.clear();
        for (size_t i = 0; i < kMaxQueuedCallbacks - 1; ++i)
            Check(QueueCallbackLocked(state, 904, NULL, 0, 0, 0, false, 0),
                  "callback pair prefill failed");
        const size_t beforePair = state.callbacks.size();
        Check(!QueueSteam2AuthCallbacksLocked(state, 77,
                                               CSteamID(0x66000001u, 1, k_EUniversePublic, k_EAccountTypeIndividual),
                                               1),
              "auth callback pair was partially accepted without two free slots");
        Check(state.callbacks.size() == beforePair, "failed auth callback pair changed queue size");
        state.callbacks.clear();
        CheckClean(state, "callback limit test leaked state");
    }
    std::printf("[PASS] bounded callback queue: %u callbacks / %u-byte payload\n",
                static_cast<unsigned>(kMaxQueuedCallbacks),
                static_cast<unsigned>(kMaxCallbackPayloadBytes));
    std::puts("[PASS] atomic 205 -> 201 pair reservation under queue saturation");

    // A callback-queue failure after identity binding must be rolled back just
    // like the production GSSendSteam2UserConnect path does.
    {
        RuntimeState state;
        const uint32 accountId = 12001;
        const auth::AuthTicketIdentity identity = MakeIdentity(0x67000001u, 0x3900000000000001ULL);
        Steam2AuthAttempt attempt = BeginSteam2AuthLocked(state, accountId);
        uint64 generation = 0;
        Check(CompleteSteam2UserLocked(state, accountId, attempt, identity, &generation)
                  == kSteam2RegistrationAccepted,
              "queue rollback setup auth failed");
        for (size_t i = 0; i < kMaxQueuedCallbacks - 1; ++i)
            Check(QueueCallbackLocked(state, 905, NULL, 0, 0, 0, false, 0),
                  "queue rollback prefill failed");
        Check(!QueueSteam2AuthCallbacksLocked(state, accountId, identity.steamID, generation),
              "queue rollback setup unexpectedly queued auth pair");
        Check(RemoveSteam2UserLocked(state, accountId, NULL, "callback_queue_full") == 1,
              "queue-full registration rollback failed");
        state.callbacks.clear();
        CheckClean(state, "queue-full registration rollback leaked state");
    }
    std::puts("[PASS] callback saturation leaves no half-authenticated session");

    // Replay flood: exactly one of many simultaneous accounts may bind one
    // exact ticket fingerprint/SteamID. Every loser must be removed.
    {
        const size_t contenders = 32;
        RuntimeState state;
        StartGate gate(contenders);
        std::atomic<int> accepted(0);
        std::atomic<int> replayed(0);
        std::atomic<int> failed(0);
        std::vector<std::thread> workers;
        const auth::AuthTicketIdentity shared = MakeIdentity(0x68000001u, 0x3900000000000100ULL);

        for (size_t i = 0; i < contenders; ++i)
        {
            workers.push_back(std::thread([&, i]() {
                const uint32 accountId = static_cast<uint32>(13000 + i);
                gate.Wait();
                std::lock_guard<std::mutex> lock(state.mutex);
                Steam2AuthAttempt attempt = BeginSteam2AuthLocked(state, accountId);
                uint64 generation = 0;
                const Steam2RegistrationResult result =
                    CompleteSteam2UserLocked(state, accountId, attempt, shared, &generation);
                if (result == kSteam2RegistrationAccepted)
                    ++accepted;
                else if (result == kSteam2RegistrationActiveReplay)
                    ++replayed;
                else
                    ++failed;
            }));
        }
        for (size_t i = 0; i < workers.size(); ++i)
            workers[i].join();
        Check(accepted.load() == 1 && replayed.load() == static_cast<int>(contenders - 1) && failed.load() == 0,
              "replay flood arbitration mismatch");
        Check(state.steam2Users.size() == 1, "replay flood leaked loser sessions");
        Check(RemoveSteam2UserLocked(state, state.steam2Users.begin()->first, NULL, "replay_flood_cleanup") == 1,
              "replay flood winner cleanup failed");
        CheckClean(state, "replay flood cleanup leaked state");
    }
    std::puts("[PASS] 32-way active ticket replay flood isolation");

    // Duplicate identity flood: same SteamID with distinct fingerprints must
    // also produce one winner and deterministic duplicate classification.
    {
        const size_t contenders = 32;
        RuntimeState state;
        StartGate gate(contenders);
        std::atomic<int> accepted(0);
        std::atomic<int> duplicates(0);
        std::atomic<int> failed(0);
        std::vector<std::thread> workers;

        for (size_t i = 0; i < contenders; ++i)
        {
            workers.push_back(std::thread([&, i]() {
                const uint32 accountId = static_cast<uint32>(14000 + i);
                const auth::AuthTicketIdentity identity =
                    MakeIdentity(0x69000001u, 0x3900000000000200ULL + static_cast<uint64>(i));
                gate.Wait();
                std::lock_guard<std::mutex> lock(state.mutex);
                Steam2AuthAttempt attempt = BeginSteam2AuthLocked(state, accountId);
                uint64 generation = 0;
                const Steam2RegistrationResult result =
                    CompleteSteam2UserLocked(state, accountId, attempt, identity, &generation);
                if (result == kSteam2RegistrationAccepted)
                    ++accepted;
                else if (result == kSteam2RegistrationDuplicateSteamID)
                    ++duplicates;
                else
                    ++failed;
            }));
        }
        for (size_t i = 0; i < workers.size(); ++i)
            workers[i].join();
        Check(accepted.load() == 1 && duplicates.load() == static_cast<int>(contenders - 1) && failed.load() == 0,
              "duplicate SteamID flood arbitration mismatch");
        Check(state.steam2Users.size() == 1, "duplicate flood leaked loser sessions");
        Check(RemoveSteam2UserLocked(state, state.steam2Users.begin()->first, NULL, "duplicate_flood_cleanup") == 1,
              "duplicate flood winner cleanup failed");
        CheckClean(state, "duplicate flood cleanup leaked state");
    }
    std::puts("[PASS] 32-way duplicate SteamID flood isolation");

    // Sustained lock contention and generation reuse. Each logical client owns
    // a distinct identity; accountId is intentionally reused 100 times.
    {
        const size_t threadCount = 32;
        const size_t cyclesPerThread = 100;
        RuntimeState state;
        StartGate gate(threadCount);
        std::atomic<int> failures(0);
        std::vector<std::thread> workers;

        for (size_t threadIndex = 0; threadIndex < threadCount; ++threadIndex)
        {
            workers.push_back(std::thread([&, threadIndex]() {
                const uint32 accountId = static_cast<uint32>(20000 + threadIndex);
                const auth::AuthTicketIdentity identity = MakeIdentity(
                    static_cast<uint32>(0x70000001u + threadIndex * 2u),
                    0x3900000010000000ULL + static_cast<uint64>(threadIndex));
                uint64 previousGeneration = 0;
                gate.Wait();

                for (size_t cycle = 0; cycle < cyclesPerThread; ++cycle)
                {
                    std::lock_guard<std::mutex> lock(state.mutex);
                    Steam2AuthAttempt attempt = BeginSteam2AuthLocked(state, accountId);
                    if (attempt.capacityExceeded)
                    {
                        ++failures;
                        return;
                    }
                    uint64 generation = 0;
                    if (CompleteSteam2UserLocked(state, accountId, attempt, identity, &generation)
                            != kSteam2RegistrationAccepted
                        || generation == 0 || generation == previousGeneration)
                    {
                        ++failures;
                        return;
                    }
                    ApplyAuthPairLocked(state, accountId, identity.steamID, generation);
                    uint64 expectedSteamID = 0;
                    uint64 removedGeneration = 0;
                    if (DisconnectSteam2UserLocked(state, accountId, identity.steamID,
                                                   &expectedSteamID, &removedGeneration)
                            != kSteam2DisconnectRemoved
                        || expectedSteamID != identity.steamID.ConvertToUint64()
                        || removedGeneration != generation)
                    {
                        ++failures;
                        return;
                    }
                    // Repeated disconnect is intentionally harmless.
                    if (DisconnectSteam2UserLocked(state, accountId, identity.steamID,
                                                   &expectedSteamID, &removedGeneration)
                            != kSteam2DisconnectAlreadyAbsent)
                    {
                        ++failures;
                        return;
                    }
                    previousGeneration = generation;
                }
            }));
        }
        for (size_t i = 0; i < workers.size(); ++i)
            workers[i].join();
        Check(failures.load() == 0, "concurrent robustness churn failed");
        CheckClean(state, "concurrent robustness churn leaked state");
        std::printf("[PASS] concurrent abuse churn: %u lifecycle operations\n",
                    static_cast<unsigned>(threadCount * cyclesPerThread));
    }

    std::puts("[PASS] final state: sessions=0 callbacks=0 in_flight=0");
    std::puts("M3.9 robustness/abuse stress PASS");
    return 0;
}
