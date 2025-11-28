#include "trdp/session.hpp"

#include "trdp/logging.hpp"

#include <chrono>
#include <thread>

#ifdef TRDP_AVAILABLE
extern "C" {
#include <trdp_if.h>
#include <trdp_utils.h>
}
#endif

namespace trdp
{

TrdpSession::TrdpSession(SessionConfig cfg) : config_(std::move(cfg)) {}

TrdpSession::~TrdpSession()
{
    close();
}

bool TrdpSession::init()
{
#ifdef TRDP_AVAILABLE
    TRDP_APP_SESSION_T appHandle{};
    TRDP_ERR_T err = tlc_init(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (err != TRDP_NO_ERR)
    {
        error("tlc_init failed with error code " + std::to_string(err));
        return false;
    }
    appHandle_ = appHandle;
    initialized_ = true;
    info("TRDP stack initialised.");
#else
    info("TRDP_AVAILABLE is not defined; running with stub session.");
    initialized_ = true;
#endif
    return true;
}

bool TrdpSession::open()
{
    if (!initialized_)
    {
        warn("init() must be called before open().");
        return false;
    }
#ifdef TRDP_AVAILABLE
    TRDP_ERR_T err = tlc_openSession(&appHandle_, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (err != TRDP_NO_ERR)
    {
        error("tlc_openSession failed with error code " + std::to_string(err));
        return false;
    }
    info("TRDP session opened on PD port " + std::to_string(config_.pdPort) + " and MD port " +
         std::to_string(config_.mdPort));
#endif
    opened_ = true;
    return true;
}

void TrdpSession::close()
{
#ifdef TRDP_AVAILABLE
    if (opened_)
    {
        tlc_closeSession(appHandle_);
        opened_ = false;
    }
    if (initialized_)
    {
        tlc_terminate();
        initialized_ = false;
    }
#else
    initialized_ = false;
    opened_ = false;
#endif
}

void TrdpSession::runOnce()
{
#ifdef TRDP_AVAILABLE
    if (!initialized_)
    {
        return;
    }
    UINT32 intervalUs{0};
    tlc_getInterval(appHandle_, &intervalUs);
    tlc_process(appHandle_);
#else
    // In stub mode we simply wait a little to simulate activity.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
#endif
}

void TrdpSession::runLoop(std::atomic_bool &runningFlag)
{
    while (runningFlag.load())
    {
        runOnce();
    }
}

} // namespace trdp

