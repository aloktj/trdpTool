#pragma once

#include "config.hpp"

#include <functional>
#include <optional>
#include <string>

namespace trdp
{

using PdUpdateCallback = std::function<void(const PdSubscribeTelegram &)>;

class PdEngine
{
public:
    explicit PdEngine(TrdpConfig &config);

    void listPublish(std::ostream &os) const;
    void listSubscribe(std::ostream &os) const;

    bool setPublishValue(std::size_t index, const std::string &element, const std::string &value);
    bool clearPublish(std::size_t index);

    void forEachPublish(const std::function<void(PdPublishTelegram &)> &fn);
    void forEachSubscribe(const std::function<void(PdSubscribeTelegram &)> &fn);

private:
    TrdpConfig &config_;
};

} // namespace trdp

