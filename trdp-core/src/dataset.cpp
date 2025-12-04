#include "trdp/dataset.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace trdp
{

std::size_t DatasetDef::payloadSize() const
{
    std::size_t size = 0;
    for (const auto &el : elements)
    {
        size = std::max(size, el.offset + std::max(el.length, expectedSize(el)) * std::max<std::size_t>(1, el.arrayLength));
    }
    return size;
}

const DatasetElementDef *DatasetDef::find(const std::string &elementName) const
{
    const auto it = std::find_if(elements.begin(), elements.end(), [&](const auto &el) { return el.name == elementName; });
    if (it == elements.end())
    {
        return nullptr;
    }
    return &(*it);
}

std::size_t expectedSize(const DatasetElementDef &def)
{
    if (def.length != 0)
    {
        return def.length;
    }
    return defaultElementSize(def.type);
}

bool assignValue(ElementValue &value, const std::string &input)
{
    if (value.locked)
    {
        warn("Element '" + value.element.name + "' is locked; skipping update.");
        return false;
    }

    std::vector<uint8_t> buffer;
    const auto size = expectedSize(value.element);

    auto appendIntegral = [&](auto parsed) {
        using T = decltype(parsed);
        buffer.resize(sizeof(T));
        std::memcpy(buffer.data(), &parsed, sizeof(T));
    };

    try
    {
        switch (value.element.type)
        {
        case TrdpType::BOOL1:
        {
            const bool parsed = (input == "1" || input == "true" || input == "True");
            buffer = {static_cast<uint8_t>(parsed)};
            break;
        }
        case TrdpType::CHAR8:
        {
            const char parsed = input.empty() ? '\0' : input.front();
            buffer = {static_cast<uint8_t>(parsed)};
            break;
        }
        case TrdpType::INT8:
            appendIntegral(static_cast<int8_t>(std::stoi(input)));
            break;
        case TrdpType::UINT8:
            appendIntegral(static_cast<uint8_t>(std::stoul(input)));
            break;
        case TrdpType::INT16:
            appendIntegral(static_cast<int16_t>(std::stoi(input)));
            break;
        case TrdpType::UINT16:
            appendIntegral(static_cast<uint16_t>(std::stoul(input)));
            break;
        case TrdpType::UINT32:
            appendIntegral(static_cast<uint32_t>(std::stoul(input)));
            break;
        case TrdpType::UINT64:
            appendIntegral(static_cast<uint64_t>(std::stoull(input)));
            break;
        case TrdpType::INT32:
            appendIntegral(static_cast<int32_t>(std::stol(input)));
            break;
        case TrdpType::INT64:
            appendIntegral(static_cast<int64_t>(std::stoll(input)));
            break;
        case TrdpType::TIMEDATE32:
            appendIntegral(static_cast<uint32_t>(std::stoul(input)));
            break;
        case TrdpType::TIMEDATE64:
            appendIntegral(static_cast<uint64_t>(std::stoull(input)));
            break;
        case TrdpType::REAL32:
        {
            const float parsed = std::stof(input);
            appendIntegral(parsed);
            break;
        }
        case TrdpType::REAL64:
        {
            const double parsed = std::stod(input);
            appendIntegral(parsed);
            break;
        }
        case TrdpType::STRING:
        case TrdpType::BYTES:
        {
            buffer.assign(input.begin(), input.end());
            if (size != 0 && buffer.size() < size)
            {
                buffer.resize(size, 0);
            }
            break;
        }
        case TrdpType::UTF16:
        {
            std::u16string parsed(input.begin(), input.end());
            buffer.resize(parsed.size() * sizeof(char16_t));
            std::memcpy(buffer.data(), parsed.data(), buffer.size());
            if (size != 0 && buffer.size() < size)
            {
                buffer.resize(size, 0);
            }
            break;
        }
        }
    }
    catch (const std::exception &ex)
    {
        error("Failed to parse value for element '" + value.element.name + "': " + ex.what());
        return false;
    }

    if (size != 0 && buffer.size() != size)
    {
        warn("Element '" + value.element.name + "' expected " + std::to_string(size) + " bytes but got " +
             std::to_string(buffer.size()) + ". Padding/truncation applied.");
        buffer.resize(size, 0);
    }

    value.rawValue = std::move(buffer);
    return true;
}

bool packDatasetToPayload(const DatasetDef &dataset, const ElementValues &values, std::vector<uint8_t> &outBuffer)
{
    const auto size = dataset.payloadSize();
    outBuffer.assign(size, 0);

    for (const auto &value : values)
    {
        const std::size_t expected = expectedSize(value.element) * std::max<std::size_t>(1, value.element.arrayLength);
        if (value.rawValue.size() < expected)
        {
            warn("Value for element '" + value.element.name + "' is smaller than expected size, padding with zeros.");
        }
        const auto copySize = std::min(expected, value.rawValue.size());
        if (value.element.offset + expected > outBuffer.size())
        {
            warn("Skipping element '" + value.element.name + "' because it does not fit in payload buffer.");
            continue;
        }
        std::copy_n(value.rawValue.begin(), copySize, outBuffer.begin() + value.element.offset);
        if (copySize < expected)
        {
            std::fill(outBuffer.begin() + value.element.offset + copySize,
                      outBuffer.begin() + value.element.offset + expected, 0);
        }
    }

    return true;
}

bool unpackPayloadToDataset(const DatasetDef &dataset, const std::vector<uint8_t> &payload, ElementValues &outValues)
{
    outValues.clear();
    for (const auto &element : dataset.elements)
    {
        const auto size = expectedSize(element) * std::max<std::size_t>(1, element.arrayLength);
        if (element.offset + size > payload.size())
        {
            warn("Payload too small to decode element '" + element.name + "'.");
            continue;
        }
        ElementValue decoded{element, {}};
        decoded.rawValue.resize(size);
        std::copy_n(payload.begin() + element.offset, size, decoded.rawValue.begin());
        outValues.push_back(std::move(decoded));
    }
    return true;
}

void DatasetRegistry::add(DatasetDef def)
{
    datasets_[def.datasetId] = std::move(def);
}

const DatasetDef *DatasetRegistry::find(uint16_t datasetId) const
{
    const auto it = datasets_.find(datasetId);
    if (it == datasets_.end())
    {
        return nullptr;
    }
    return &it->second;
}

std::vector<DatasetDef> DatasetRegistry::list() const
{
    std::vector<DatasetDef> result;
    result.reserve(datasets_.size());
    for (const auto &pair : datasets_)
    {
        result.push_back(pair.second);
    }
    return result;
}

} // namespace trdp

