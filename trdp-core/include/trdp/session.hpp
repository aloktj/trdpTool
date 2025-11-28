#pragma once

#include "config.hpp"
#include "logging.hpp"

#include <atomic>
#include <optional>
#include <string>
#include <thread>

namespace trdp
{

struct SessionConfig
{
    std::string processName{"trdp-sim"};
    std::string localIp{"0.0.0.0"};
    uint16_t pdPort{17224};
    uint16_t mdPort{17225};
    uint32_t timeoutUs{100000};
};

class TrdpSession
{
public:
    explicit TrdpSession(SessionConfig cfg = {});
    ~TrdpSession();

    bool init();
    bool open();
    void close();

    void runOnce();
    void runLoop(std::atomic_bool &runningFlag);

private:
    SessionConfig config_;
#ifdef TRDP_AVAILABLE
    TRDP_APP_SESSION_T appHandle_{};
#endif
    bool initialized_{false};
    bool opened_{false};
};

} // namespace trdp

