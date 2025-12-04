#include "trdp/pd.hpp"

#include "trdp/dataset.hpp"
#include "trdp/logging.hpp"
#include "trdp/tau.hpp"

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
        if (val.locked)
        {
            warn("Skipping clear for locked element '" + val.element.name + "'");
            continue;
        }
        val.rawValue.assign(val.rawValue.size(), 0);
    }
    return true;
}

bool PdEngine::setPublishLock(std::size_t index, const std::string &element, bool locked)
{
    if (index >= config_.pdPublish.size())
    {
        return false;
    }
    auto &pub = config_.pdPublish[index];
    for (auto &val : pub.values)
    {
        if (val.element.name == element)
        {
            val.locked = locked;
            return true;
        }
    }
    return false;
}

bool PdEngine::buildPublishPayload(std::size_t index, std::vector<uint8_t> &networkPayload) const
{
    if (index >= config_.pdPublish.size())
    {
        return false;
    }

    const auto &pub = config_.pdPublish[index];
    const auto *dataset = config_.datasetRegistry.find(pub.datasetId);
    if (dataset == nullptr)
    {
        warn("Unknown dataset for publish payload: " + std::to_string(pub.datasetId));
        return false;
    }

    std::vector<uint8_t> hostPayload;
    packDatasetToPayload(*dataset, pub.values, hostPayload);

    if (config_.tauMarshaller && config_.tauMarshaller->valid())
    {
        std::vector<uint8_t> marshalled;
        if (config_.tauMarshaller->marshall(pub.comId, hostPayload, marshalled))
        {
            networkPayload = std::move(marshalled);
            return true;
        }
        warn("Falling back to raw payload after failed tau_marshall for ComId " + std::to_string(pub.comId));
    }

    networkPayload = std::move(hostPayload);
    return true;
}

bool PdEngine::updateSubscribeValues(std::size_t index, const std::vector<uint8_t> &networkPayload)
{
    if (index >= config_.pdSubscribe.size())
    {
        return false;
    }

    auto &sub = config_.pdSubscribe[index];
    const auto *dataset = config_.datasetRegistry.find(sub.datasetId);
    if (dataset == nullptr)
    {
        warn("Unknown dataset for subscribe payload: " + std::to_string(sub.datasetId));
        return false;
    }

    std::vector<uint8_t> hostPayload;
    if (config_.tauMarshaller && config_.tauMarshaller->valid())
    {
        if (!config_.tauMarshaller->unmarshall(sub.comId, networkPayload, hostPayload))
        {
            warn("Failed to apply tau_unmarshall for subscribe ComId " + std::to_string(sub.comId));
            return false;
        }
    }
    else
    {
        hostPayload = networkPayload;
    }

    return unpackPayloadToDataset(*dataset, hostPayload, sub.lastValues);
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

