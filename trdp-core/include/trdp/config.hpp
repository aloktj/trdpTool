#pragma once

#include "dataset.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace trdp
{

class TauMarshaller;

struct PdPublishTelegram
{
    uint32_t comId{0};
    uint16_t datasetId{0};
    std::string destinationIp;
    uint32_t cycleTimeMs{1000};
    uint32_t priority{3};
    ElementValues values;
};

struct PdSubscribeTelegram
{
    uint32_t comId{0};
    uint16_t datasetId{0};
    std::string sourceIp;
    std::string destinationIp;
    uint32_t timeoutMs{1000};
    ElementValues lastValues;
};

enum class MdDirection
{
    Request,
    Reply,
    Notify,
    Confirm
};

struct MdTemplate
{
    std::string name;
    MdDirection direction{MdDirection::Request};
    uint32_t comId{0};
    uint16_t datasetId{0};
    std::string destinationIp;
    uint16_t destinationPort{0u};
    ElementValues values;
};

struct TrdpConfig
{
    DatasetRegistry datasetRegistry;
    std::vector<PdPublishTelegram> pdPublish;
    std::vector<PdSubscribeTelegram> pdSubscribe;
    std::vector<MdTemplate> mdTemplates;
    std::shared_ptr<TauMarshaller> tauMarshaller;
};

class XmlConfigLoader
{
public:
    std::optional<TrdpConfig> loadFromDeviceConfig(const std::string &deviceFile,
                                                   const std::string &comIdFile,
                                                   const std::string &pdFile,
                                                   const std::string &mdFile) const;
};

} // namespace trdp

