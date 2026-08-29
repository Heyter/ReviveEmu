#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "../LegacySteamAuth.h"
#include "../LegacySteamCallbacks.h"
#include "../LegacySteamLifecycle.h"
#include "../LegacySteamRuntime.h"

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
    std::fprintf(stderr, "multi-client stress FAIL: %s\n", message ? message : "unknown error");
    std::exit(1);
}

void Check(bool condition, const char *message)
{
    if (!condition)
        Fail(message);
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
    const QueuedCallback callback205 = AuthCallback(kCallbackGSClientSteam2Accept, accountId, steamID64, generation);
    const QueuedCallback callback201 = AuthCallback(kCallbackGSClientApprove, accountId, steamID64, generation);
    ApplyAuthCallbackLifecycleLocked(state, callback205);
    ApplyAuthCallbackLifecycleLocked(state, callback201);
}

void CheckAllActiveLocked(const RuntimeState &state, size_t expected)
{
    Check(state.steam2Users.size() == expected, "unexpected active session count");
    for (std::map<uint32, Steam2AuthSession>::const_iterator it = state.steam2Users.begin();
         it != state.steam2Users.end(); ++it)
    {
        Check(it->second.identityBound, "active session has no ticket identity");
        Check(it->second.lifecycle == kClientLifecycleActive, "session did not reach ACTIVE");
        Check(it->second.generation != 0, "active session has zero generation");
    }
}

} // namespace

int main()
{
#if !defined(_WIN32)
    setenv("REVIVE_STEAMCLIENT_LOG", "/dev/null", 1);
#endif

    const size_t clientCount = 16;
    RuntimeState state;
    StartGate gate(clientCount);
    std::atomic<int> failures(0);
    std::vector<std::thread> workers;
    std::vector<uint64> generations(clientCount, 0);
    std::vector<uint64> steamIDs(clientCount, 0);

    for (size_t i = 0; i < clientCount; ++i)
    {
        workers.push_back(std::thread([&, i]() {
            const uint32 accountId = static_cast<uint32>(1000 + i);
            const auth::AuthTicketIdentity identity = MakeIdentity(
                static_cast<uint32>(0x22000000u + i * 2u + 1u),
                0xA100000000000000ULL + static_cast<uint64>(i + 1));

            gate.Wait();

            Steam2AuthAttempt attempt;
            {
                std::lock_guard<std::mutex> lock(state.mutex);
                attempt = BeginSteam2AuthLocked(state, accountId);
            }
            std::this_thread::yield();

            uint64 generation = 0;
            Steam2RegistrationResult result;
            {
                std::lock_guard<std::mutex> lock(state.mutex);
                result = CompleteSteam2UserLocked(state, accountId, attempt, identity, &generation);
                if (result == kSteam2RegistrationAccepted)
                    QueueSteam2AuthCallbacksLocked(state, accountId, identity.steamID, generation);
            }

            if (result != kSteam2RegistrationAccepted || generation == 0)
            {
                ++failures;
                return;
            }
            generations[i] = generation;
            steamIDs[i] = identity.steamID.ConvertToUint64();
        }));
    }

    for (size_t i = 0; i < workers.size(); ++i)
        workers[i].join();
    Check(failures.load() == 0, "concurrent unique authentication failed");

    {
        std::lock_guard<std::mutex> lock(state.mutex);
        Check(state.steam2Users.size() == clientCount, "not all concurrent clients were retained");
        Check(state.callbacks.size() == clientCount * 2, "auth callback pairs were lost under concurrency");

        std::map<uint32, int> callbackStage;
        while (!state.callbacks.empty())
        {
            const QueuedCallback callback = state.callbacks.front();
            state.callbacks.pop_front();
            int &stage = callbackStage[callback.accountId];
            if (callback.callback == kCallbackGSClientSteam2Accept)
            {
                Check(stage == 0, "callback 205 was duplicated or reordered for a client");
                stage = 1;
            }
            else if (callback.callback == kCallbackGSClientApprove)
            {
                Check(stage == 1, "callback 201 arrived before callback 205");
                stage = 2;
            }
            else
            {
                Fail("unexpected callback in multi-client auth queue");
            }
            ApplyAuthCallbackLifecycleLocked(state, callback);
        }
        Check(callbackStage.size() == clientCount, "callback coverage did not include every client");
        for (std::map<uint32, int>::const_iterator it = callbackStage.begin(); it != callbackStage.end(); ++it)
            Check(it->second == 2, "client did not receive a complete 205 -> 201 pair");
        CheckAllActiveLocked(state, clientCount);
    }

    {
        std::set<uint64> generationSet(generations.begin(), generations.end());
        std::set<uint64> steamIDSet(steamIDs.begin(), steamIDs.end());
        Check(generationSet.size() == clientCount, "lifecycle generations are not unique");
        Check(steamIDSet.size() == clientCount, "SteamIDs are not unique");
    }

    std::puts("[PASS] 16 concurrent synthetic client sessions");
    std::puts("[PASS] unique SteamID/session isolation");
    std::puts("[PASS] independent lifecycle generations and callback pairs");

    // Disconnect one client and prove the other 15 remain ACTIVE. Then reuse
    // the same accountId after cleanup and require a fresh lifecycle generation.
    const size_t isolatedIndex = 7;
    const uint32 isolatedAccount = static_cast<uint32>(1000 + isolatedIndex);
    const CSteamID isolatedSteamID(steamIDs[isolatedIndex]);
    const uint64 oldGeneration = generations[isolatedIndex];
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        uint64 expectedSteamID = 0;
        uint64 removedGeneration = 0;
        Check(DisconnectSteam2UserLocked(state, isolatedAccount, isolatedSteamID,
                                         &expectedSteamID, &removedGeneration) == kSteam2DisconnectRemoved,
              "isolated disconnect failed");
        Check(expectedSteamID == isolatedSteamID.ConvertToUint64(), "isolated disconnect SteamID mismatch");
        Check(removedGeneration == oldGeneration, "isolated disconnect generation mismatch");
        CheckAllActiveLocked(state, clientCount - 1);
    }

    auth::AuthTicketIdentity isolatedIdentity;
    isolatedIdentity.type = auth::kAuthTicketClassicRevEmu;
    isolatedIdentity.steamID = isolatedSteamID;
    isolatedIdentity.fingerprint = 0xA100000000000000ULL + static_cast<uint64>(isolatedIndex + 1);
    uint64 newGeneration = 0;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        Steam2AuthAttempt attempt = BeginSteam2AuthLocked(state, isolatedAccount);
        Check(CompleteSteam2UserLocked(state, isolatedAccount, attempt, isolatedIdentity, &newGeneration)
                  == kSteam2RegistrationAccepted,
              "accountId reuse after isolated disconnect failed");
        Check(newGeneration != oldGeneration, "accountId reuse did not allocate a fresh generation");
        ApplyAuthPairLocked(state, isolatedAccount, isolatedSteamID, newGeneration);
        CheckAllActiveLocked(state, clientCount);
    }
    std::puts("[PASS] single-client disconnect isolation");
    std::puts("[PASS] accountId reuse after cleanup");

    // Two simultaneous accounts racing for one exact ticket: exactly one may
    // bind it; the other must be classified as an active replay.
    {
        RuntimeState replayState;
        const auth::AuthTicketIdentity sharedIdentity = MakeIdentity(0x33000001u, 0xB200000000000001ULL);
        Steam2AuthAttempt attempts[2];
        {
            std::lock_guard<std::mutex> lock(replayState.mutex);
            attempts[0] = BeginSteam2AuthLocked(replayState, 2001);
            attempts[1] = BeginSteam2AuthLocked(replayState, 2002);
        }
        StartGate replayGate(2);
        Steam2RegistrationResult results[2] = {kSteam2RegistrationAuthCanceled, kSteam2RegistrationAuthCanceled};
        std::thread a([&]() {
            replayGate.Wait();
            uint64 generation = 0;
            std::lock_guard<std::mutex> lock(replayState.mutex);
            results[0] = CompleteSteam2UserLocked(replayState, 2001, attempts[0], sharedIdentity, &generation);
        });
        std::thread b([&]() {
            replayGate.Wait();
            uint64 generation = 0;
            std::lock_guard<std::mutex> lock(replayState.mutex);
            results[1] = CompleteSteam2UserLocked(replayState, 2002, attempts[1], sharedIdentity, &generation);
        });
        a.join();
        b.join();
        const int accepted = (results[0] == kSteam2RegistrationAccepted ? 1 : 0)
                           + (results[1] == kSteam2RegistrationAccepted ? 1 : 0);
        const int replayed = (results[0] == kSteam2RegistrationActiveReplay ? 1 : 0)
                           + (results[1] == kSteam2RegistrationActiveReplay ? 1 : 0);
        Check(accepted == 1 && replayed == 1, "active replay arbitration was not atomic");
        std::lock_guard<std::mutex> lock(replayState.mutex);
        Check(replayState.steam2Users.size() == 1, "replay loser leaked a client session");
    }

    // Same SteamID but a different ticket fingerprint is a duplicate identity,
    // not an exact-ticket replay. Again exactly one concurrent contender wins.
    {
        RuntimeState duplicateState;
        const uint32 steamAccount = 0x44000001u;
        const auth::AuthTicketIdentity identities[2] = {
            MakeIdentity(steamAccount, 0xC300000000000001ULL),
            MakeIdentity(steamAccount, 0xC300000000000002ULL)
        };
        Steam2AuthAttempt attempts[2];
        {
            std::lock_guard<std::mutex> lock(duplicateState.mutex);
            attempts[0] = BeginSteam2AuthLocked(duplicateState, 3001);
            attempts[1] = BeginSteam2AuthLocked(duplicateState, 3002);
        }
        StartGate duplicateGate(2);
        Steam2RegistrationResult results[2] = {kSteam2RegistrationAuthCanceled, kSteam2RegistrationAuthCanceled};
        std::thread a([&]() {
            duplicateGate.Wait();
            uint64 generation = 0;
            std::lock_guard<std::mutex> lock(duplicateState.mutex);
            results[0] = CompleteSteam2UserLocked(duplicateState, 3001, attempts[0], identities[0], &generation);
        });
        std::thread b([&]() {
            duplicateGate.Wait();
            uint64 generation = 0;
            std::lock_guard<std::mutex> lock(duplicateState.mutex);
            results[1] = CompleteSteam2UserLocked(duplicateState, 3002, attempts[1], identities[1], &generation);
        });
        a.join();
        b.join();
        const int accepted = (results[0] == kSteam2RegistrationAccepted ? 1 : 0)
                           + (results[1] == kSteam2RegistrationAccepted ? 1 : 0);
        const int duplicates = (results[0] == kSteam2RegistrationDuplicateSteamID ? 1 : 0)
                             + (results[1] == kSteam2RegistrationDuplicateSteamID ? 1 : 0);
        Check(accepted == 1 && duplicates == 1, "duplicate SteamID arbitration was not atomic");
        std::lock_guard<std::mutex> lock(duplicateState.mutex);
        Check(duplicateState.steam2Users.size() == 1, "duplicate loser leaked a client session");
    }
    std::puts("[PASS] concurrent duplicate SteamID / active-ticket replay isolation");

    // Concurrent churn: several logical clients repeatedly authenticate,
    // become ACTIVE, disconnect and immediately reuse their accountId. This
    // exercises generation allocation and cleanup under sustained contention.
    {
        const size_t churnThreads = 8;
        const size_t cyclesPerThread = 64;
        RuntimeState churnState;
        StartGate churnGate(churnThreads);
        std::atomic<int> churnFailures(0);
        std::vector<std::thread> churnWorkers;

        for (size_t threadIndex = 0; threadIndex < churnThreads; ++threadIndex)
        {
            churnWorkers.push_back(std::thread([&, threadIndex]() {
                const uint32 accountId = static_cast<uint32>(5000 + threadIndex);
                const auth::AuthTicketIdentity identity = MakeIdentity(
                    static_cast<uint32>(0x55000001u + threadIndex * 2u),
                    0xD400000000000000ULL + static_cast<uint64>(threadIndex + 1));
                uint64 previousGeneration = 0;
                churnGate.Wait();

                for (size_t cycle = 0; cycle < cyclesPerThread; ++cycle)
                {
                    Steam2AuthAttempt attempt;
                    {
                        std::lock_guard<std::mutex> lock(churnState.mutex);
                        attempt = BeginSteam2AuthLocked(churnState, accountId);
                    }
                    std::this_thread::yield();

                    uint64 generation = 0;
                    {
                        std::lock_guard<std::mutex> lock(churnState.mutex);
                        if (CompleteSteam2UserLocked(churnState, accountId, attempt, identity, &generation)
                            != kSteam2RegistrationAccepted)
                        {
                            ++churnFailures;
                            return;
                        }
                        if (generation == 0 || generation == previousGeneration)
                        {
                            ++churnFailures;
                            return;
                        }
                        ApplyAuthPairLocked(churnState, accountId, identity.steamID, generation);
                        std::map<uint32, Steam2AuthSession>::const_iterator active =
                            churnState.steam2Users.find(accountId);
                        if (active == churnState.steam2Users.end()
                            || active->second.lifecycle != kClientLifecycleActive)
                        {
                            ++churnFailures;
                            return;
                        }
                    }
                    previousGeneration = generation;
                    std::this_thread::yield();

                    {
                        std::lock_guard<std::mutex> lock(churnState.mutex);
                        uint64 expectedSteamID = 0;
                        uint64 removedGeneration = 0;
                        if (DisconnectSteam2UserLocked(churnState, accountId, identity.steamID,
                                                       &expectedSteamID, &removedGeneration)
                                != kSteam2DisconnectRemoved
                            || removedGeneration != generation)
                        {
                            ++churnFailures;
                            return;
                        }
                    }
                }
            }));
        }

        for (size_t i = 0; i < churnWorkers.size(); ++i)
            churnWorkers[i].join();
        Check(churnFailures.load() == 0, "concurrent connect/disconnect churn failed");
        {
            std::lock_guard<std::mutex> lock(churnState.mutex);
            Check(churnState.steam2Users.empty(), "churn leaked client sessions");
            Check(churnState.callbacks.empty(), "churn leaked callbacks");
        }
        std::printf("[PASS] concurrent churn stress: %u sessions\n",
                    static_cast<unsigned>(churnThreads * cyclesPerThread));
    }

    std::puts("M3.8 multi-client stress PASS");
    return 0;
}
