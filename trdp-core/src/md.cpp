#include "trdp/md.hpp"

#include "trdp/dataset.hpp"
#include "trdp/logging.hpp"
#include "trdp/tau.hpp"

#include <algorithm>
#include <iostream>

namespace trdp
{

MdEngine::MdEngine(TrdpConfig &config) : config_(config) {}

void MdEngine::listTemplates(std::ostream &os) const
{
    for (const auto &tpl : config_.mdTemplates)
    {
        os << tpl.name << " COMID=" << tpl.comId << " dataset=" << tpl.datasetId << " dest=" << tpl.destinationIp
           << ":" << tpl.destinationPort << std::endl;
    }
}

bool MdEngine::setTemplateValue(const std::string &name, const std::string &element, const std::string &value)
{
    const auto it = std::find_if(config_.mdTemplates.begin(), config_.mdTemplates.end(),
                                 [&](const auto &tpl) { return tpl.name == name; });
    if (it == config_.mdTemplates.end())
    {
        return false;
    }
    for (auto &val : it->values)
    {
        if (val.element.name == element)
        {
            return assignValue(val, value);
        }
    }
    return false;
}

bool MdEngine::clearTemplate(const std::string &name)
{
    const auto it = std::find_if(config_.mdTemplates.begin(), config_.mdTemplates.end(),
                                 [&](const auto &tpl) { return tpl.name == name; });
    if (it == config_.mdTemplates.end())
    {
        return false;
    }
    for (auto &val : it->values)
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

bool MdEngine::setTemplateLock(const std::string &name, const std::string &element, bool locked)
{
    const auto it = std::find_if(config_.mdTemplates.begin(), config_.mdTemplates.end(),
                                 [&](const auto &tpl) { return tpl.name == name; });
    if (it == config_.mdTemplates.end())
    {
        return false;
    }
    for (auto &val : it->values)
    {
        if (val.element.name == element)
        {
            val.locked = locked;
            return true;
        }
    }
    return false;
}

bool MdEngine::sendTemplate(const std::string &name, std::ostream &os) const
{
    const auto it = std::find_if(config_.mdTemplates.begin(), config_.mdTemplates.end(),
                                 [&](const auto &tpl) { return tpl.name == name; });
    if (it == config_.mdTemplates.end())
    {
        return false;
    }

    std::vector<uint8_t> payload;
    const DatasetDef *dataset = config_.datasetRegistry.find(it->datasetId);
    if (dataset)
    {
        packDatasetToPayload(*dataset, it->values, payload);
    }

    std::vector<uint8_t> networkPayload;
    if (config_.tauMarshaller && config_.tauMarshaller->valid())
    {
        if (!config_.tauMarshaller->marshall(it->comId, payload, networkPayload))
        {
            warn("tau_marshall failed for MD template '" + it->name + "', using host payload");
            networkPayload = payload;
        }
    }
    else
    {
        networkPayload = payload;
    }

    os << "MD send: " << it->name << " COMID=" << it->comId << " bytes=" << networkPayload.size() << std::endl;
    return true;
}

} // namespace trdp

