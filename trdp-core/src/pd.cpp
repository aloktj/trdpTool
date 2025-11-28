#include "trdp/pd.hpp"

#include "trdp/dataset.hpp"
#include "trdp/logging.hpp"

#include <iostream>

namespace trdp
{

PdEngine::PdEngine(TrdpConfig &config) : config_(config) {}

void PdEngine::listPublish(std::ostream &os) const
{
    for (std::size_t i = 0; i < config_.pdPublish.size(); ++i)
    {
        const auto &pub = config_.pdPublish[i];
        os << "#" << i << " COMID=" << pub.comId << " dataset=" << pub.datasetId << " dest=" << pub.destinationIp
           << " cycle=" << pub.cycleTimeMs << "ms" << std::endl;
        for (const auto &val : pub.values)
        {
            os << "    " << val.element.name << " (" << toString(val.element.type) << ") len="
               << val.rawValue.size() << std::endl;
        }
    }
}

void PdEngine::listSubscribe(std::ostream &os) const
{
    for (std::size_t i = 0; i < config_.pdSubscribe.size(); ++i)
    {
        const auto &sub = config_.pdSubscribe[i];
        os << "#" << i << " COMID=" << sub.comId << " dataset=" << sub.datasetId << " src=" << sub.sourceIp
           << " dest=" << sub.destinationIp << " timeout=" << sub.timeoutMs << "ms" << std::endl;
    }
}

bool PdEngine::setPublishValue(std::size_t index, const std::string &element, const std::string &value)
{
    if (index >= config_.pdPublish.size())
    {
        error("Publish index out of range");
        return false;
    }
    auto &pub = config_.pdPublish[index];
    for (auto &val : pub.values)
    {
        if (val.element.name == element)
        {
            return assignValue(val, value);
        }
    }
    warn("Element '" + element + "' not found in publish dataset.");
    return false;
}

bool PdEngine::clearPublish(std::size_t index)
{
    if (index >= config_.pdPublish.size())
    {
        return false;
    }
    auto &pub = config_.pdPublish[index];
    for (auto &val : pub.values)
    {
        val.rawValue.assign(expectedSize(val.element), 0);
    }
    return true;
}

void PdEngine::forEachPublish(const std::function<void(PdPublishTelegram &)> &fn)
{
    for (auto &pub : config_.pdPublish)
    {
        fn(pub);
    }
}

void PdEngine::forEachSubscribe(const std::function<void(PdSubscribeTelegram &)> &fn)
{
    for (auto &sub : config_.pdSubscribe)
    {
        fn(sub);
    }
}

} // namespace trdp

