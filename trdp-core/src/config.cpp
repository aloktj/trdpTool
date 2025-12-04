#include "trdp/config.hpp"

#include "trdp/logging.hpp"
#include "trdp/tau.hpp"

#include "tinyxml2.h"

#include <algorithm>
#include <cctype>
#include <optional>

namespace trdp
{
namespace
{
TrdpType parseType(const std::string &type)
{
    std::string upper = type;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    if (upper == "BOOL1" || upper == "BOOL8")
    {
        return TrdpType::BOOL1;
    }
    if (upper == "CHAR8")
    {
        return TrdpType::CHAR8;
    }
    if (upper == "INT8")
    {
        return TrdpType::INT8;
    }
    if (upper == "UINT8")
    {
        return TrdpType::UINT8;
    }
    if (upper == "INT16")
    {
        return TrdpType::INT16;
    }
    if (upper == "UINT16")
    {
        return TrdpType::UINT16;
    }
    if (upper == "INT32")
    {
        return TrdpType::INT32;
    }
    if (upper == "UINT32")
    {
        return TrdpType::UINT32;
    }
    if (upper == "INT64")
    {
        return TrdpType::INT64;
    }
    if (upper == "UINT64")
    {
        return TrdpType::UINT64;
    }
    if (upper == "REAL32")
    {
        return TrdpType::REAL32;
    }
    if (upper == "REAL64")
    {
        return TrdpType::REAL64;
    }
    if (upper == "TIMEDATE32")
    {
        return TrdpType::TIMEDATE32;
    }
    if (upper == "TIMEDATE64")
    {
        return TrdpType::TIMEDATE64;
    }
    if (upper == "STRING")
    {
        return TrdpType::STRING;
    }
    if (upper == "UTF16")
    {
        return TrdpType::UTF16;
    }
    if (upper == "BYTES")
    {
        return TrdpType::BYTES;
    }

    warn("Unknown element type '" + type + "', defaulting to BYTES");
    return TrdpType::BYTES;
}

std::string readStringAttribute(const tinyxml2::XMLElement &element, const char *name)
{
    const char *value = element.Attribute(name);
    return value != nullptr ? std::string(value) : std::string();
}

std::optional<uint32_t> readUintAttribute(const tinyxml2::XMLElement &element, const char *name)
{
    uint32_t value = 0;
    if (element.QueryUnsignedAttribute(name, &value) == tinyxml2::XML_SUCCESS)
    {
        return value;
    }
    return std::nullopt;
}

DatasetElementDef parseElement(const tinyxml2::XMLElement &element, std::size_t &currentOffset)
{
    DatasetElementDef def;
    def.name = readStringAttribute(element, "name");
    def.type = parseType(readStringAttribute(element, "type"));
    def.arrayLength = readUintAttribute(element, "array-size").value_or(1u);
    def.length = readUintAttribute(element, "size").value_or(0u);
    def.offset = currentOffset;

    const auto elementSize = expectedSize(def) * std::max<std::size_t>(1, def.arrayLength);
    if (elementSize != 0)
    {
        currentOffset += elementSize;
    }
    return def;
}

std::optional<DatasetDef> parseDataset(const tinyxml2::XMLElement &datasetElement)
{
    DatasetDef dataset;
    const auto id = readUintAttribute(datasetElement, "id");
    if (!id)
    {
        error("Dataset missing required 'id' attribute");
        return std::nullopt;
    }
    dataset.datasetId = static_cast<uint16_t>(*id);
    dataset.name = readStringAttribute(datasetElement, "name");

    std::size_t offset = 0;
    for (auto *element = datasetElement.FirstChildElement("element"); element != nullptr;
         element = element->NextSiblingElement("element"))
    {
        dataset.elements.push_back(parseElement(*element, offset));
    }

    if (dataset.elements.empty())
    {
        warn("Dataset '" + dataset.name + "' has no elements defined");
    }

    return dataset;
}

ElementValues defaultValues(const DatasetDef &dataset)
{
    ElementValues values;
    for (const auto &element : dataset.elements)
    {
        ElementValue value{element, {}};
        const auto size = expectedSize(element) * std::max<std::size_t>(1, element.arrayLength);
        value.rawValue.assign(size, 0);
        values.push_back(std::move(value));
    }
    return values;
}

std::string parseUri(const tinyxml2::XMLElement *element)
{
    if (element == nullptr)
    {
        return {};
    }
    auto uri = readStringAttribute(*element, "uri");
    if (!uri.empty())
    {
        return uri;
    }
    return readStringAttribute(*element, "uri1");
}

uint32_t toMilliseconds(uint32_t microseconds)
{
    return std::max(1u, microseconds / 1000u);
}

void parseTelegrams(const tinyxml2::XMLElement &device, TrdpConfig &config)
{
    const auto *busList = device.FirstChildElement("bus-interface-list");
    if (busList == nullptr)
    {
        warn("No <bus-interface-list> found in device configuration");
        return;
    }

    for (auto *bus = busList->FirstChildElement("bus-interface"); bus != nullptr;
         bus = bus->NextSiblingElement("bus-interface"))
    {
        for (auto *telegram = bus->FirstChildElement("telegram"); telegram != nullptr;
             telegram = telegram->NextSiblingElement("telegram"))
        {
            const auto comId = readUintAttribute(*telegram, "com-id");
            const auto datasetId = readUintAttribute(*telegram, "data-set-id");
            if (!comId || !datasetId)
            {
                warn("Skipping telegram missing required com-id or data-set-id");
                continue;
            }

            const auto *dataset = config.datasetRegistry.find(static_cast<uint16_t>(*datasetId));
            if (dataset == nullptr)
            {
                warn("Telegram references unknown dataset id " + std::to_string(*datasetId));
                continue;
            }

            const auto *pdParams = telegram->FirstChildElement("pd-parameter");
            const uint32_t cycleMicro = pdParams != nullptr ? readUintAttribute(*pdParams, "cycle").value_or(1000000u) : 1000000u;
            const uint32_t timeoutMicro = pdParams != nullptr ? readUintAttribute(*pdParams, "timeout").value_or(1000000u) : 1000000u;

            const auto destinationIp = parseUri(telegram->FirstChildElement("destination"));
            const auto sourceIp = parseUri(telegram->FirstChildElement("source"));

            PdPublishTelegram pub{};
            pub.comId = *comId;
            pub.datasetId = static_cast<uint16_t>(*datasetId);
            pub.destinationIp = destinationIp;
            pub.cycleTimeMs = toMilliseconds(cycleMicro);
            pub.values = defaultValues(*dataset);
            config.pdPublish.push_back(std::move(pub));

            PdSubscribeTelegram sub{};
            sub.comId = *comId;
            sub.datasetId = static_cast<uint16_t>(*datasetId);
            sub.sourceIp = sourceIp;
            sub.destinationIp = destinationIp;
            sub.timeoutMs = toMilliseconds(timeoutMicro);
            config.pdSubscribe.push_back(std::move(sub));
        }
    }
}

} // namespace

std::optional<TrdpConfig> XmlConfigLoader::loadFromDeviceConfig(const std::string &deviceFile,
                                                                const std::string &comIdFile,
                                                                const std::string &pdFile,
                                                                const std::string &mdFile) const
{
    (void)comIdFile;
    (void)pdFile;
    (void)mdFile;

    if (deviceFile.empty())
    {
        error("Device configuration path is empty");
        return std::nullopt;
    }

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(deviceFile.c_str()) != tinyxml2::XML_SUCCESS)
    {
        error("Failed to load device XML: " + std::string(doc.ErrorStr()));
        return std::nullopt;
    }

    const auto *device = doc.FirstChildElement("device");
    if (device == nullptr)
    {
        error("XML does not contain a <device> root element");
        return std::nullopt;
    }

    TrdpConfig config;
    const auto *datasetList = device->FirstChildElement("data-set-list");
    if (datasetList != nullptr)
    {
        for (auto *dataset = datasetList->FirstChildElement("data-set"); dataset != nullptr;
             dataset = dataset->NextSiblingElement("data-set"))
        {
            auto parsed = parseDataset(*dataset);
            if (parsed)
            {
                config.datasetRegistry.add(*parsed);
            }
        }
    }
    else
    {
        warn("Device XML does not contain a <data-set-list>");
    }

    parseTelegrams(*device, config);

    if (config.datasetRegistry.list().empty())
    {
        warn("No datasets loaded from XML");
    }

#ifdef TRDP_AVAILABLE
    config.tauMarshaller = TauMarshaller::createFromXml(deviceFile);
    if (!config.tauMarshaller)
    {
        warn("TRDP stack available but tau marshalling could not be initialised from XML.");
    }
#endif

    info("Loaded configuration from standard TRDP device XML: " + deviceFile);
    return config;
}

} // namespace trdp

