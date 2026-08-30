#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "../LegacySteamRuntime.h"

namespace
{

void Check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

std::string ReadAll(const char *path)
{
    std::ifstream input(path);
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}

} // namespace

int main()
{
    const char *path = "/tmp/revive_m40_production_smoke.log";
    std::remove(path);

    try
    {
        Check(setenv("REVIVE_STEAMCLIENT_LOG", path, 1) == 0, "failed to set log path");
        Check(setenv("REVIVE_LOG_MODE", "production", 1) == 0, "failed to enable production log mode");

        revive::legacy::Log("Auth", "production-auth-visible");
        revive::legacy::Log("Lifecycle", "production-lifecycle-visible");
        revive::legacy::Log("Interface", "diagnostic-interface-hidden");
        revive::legacy::Log("Callback", "diagnostic-callback-hidden");
        revive::legacy::Log("GameServer", "diagnostic-gameserver-hidden");

        std::string production = ReadAll(path);
        Check(production.find("production-auth-visible") != std::string::npos,
              "production mode hid Auth logging");
        Check(production.find("production-lifecycle-visible") != std::string::npos,
              "production mode hid Lifecycle logging");
        Check(production.find("diagnostic-interface-hidden") == std::string::npos,
              "production mode leaked Interface diagnostics");
        Check(production.find("diagnostic-callback-hidden") == std::string::npos,
              "production mode leaked Callback diagnostics");
        Check(production.find("diagnostic-gameserver-hidden") == std::string::npos,
              "production mode leaked GameServer diagnostics");

        Check(setenv("REVIVE_LOG_MODE", "off", 1) == 0, "failed to enable off log mode");
        const size_t beforeOff = production.size();
        revive::legacy::Log("Auth", "off-auth-hidden");
        Check(ReadAll(path).size() == beforeOff, "off mode still wrote a log record");

        Check(setenv("REVIVE_LOG_MODE", "diagnostic", 1) == 0, "failed to enable diagnostic log mode");
        revive::legacy::Log("Callback", "diagnostic-callback-visible");
        const std::string diagnostic = ReadAll(path);
        Check(diagnostic.find("diagnostic-callback-visible") != std::string::npos,
              "diagnostic mode did not restore full logging");

        std::remove(path);
        std::cout << "[PASS] production logging keeps Auth/Lifecycle and suppresses noisy components\n";
        std::cout << "[PASS] off logging mode suppresses all records\n";
        std::cout << "[PASS] diagnostic logging restores full component output\n";
        std::cout << "M4.0 production logging smoke PASS\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::remove(path);
        std::cerr << "[FAIL] " << e.what() << '\n';
        return 1;
    }
}
