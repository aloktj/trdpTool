#include "trdp/md.hpp"

#include "trdp/dataset.hpp"
#include "trdp/logging.hpp"

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
        val.rawValue.assign(expectedSize(val.element), 0);
    }
    return true;
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

    os << "MD send: " << it->name << " COMID=" << it->comId << " bytes=" << payload.size() << std::endl;
    return true;
}

} // namespace trdp

