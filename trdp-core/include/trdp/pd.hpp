#pragma once

#include "config.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

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
    bool setPublishLock(std::size_t index, const std::string &element, bool locked);

    bool buildPublishPayload(std::size_t index, std::vector<uint8_t> &networkPayload) const;
    bool updateSubscribeValues(std::size_t index, const std::vector<uint8_t> &networkPayload);

    void forEachPublish(const std::function<void(PdPublishTelegram &)> &fn);
    void forEachSubscribe(const std::function<void(PdSubscribeTelegram &)> &fn);

    const std::vector<PdPublishTelegram> &publishTelegrams() const { return config_.pdPublish; }
    const DatasetRegistry &datasets() const { return config_.datasetRegistry; }

private:
    TrdpConfig &config_;
};

} // namespace trdp

