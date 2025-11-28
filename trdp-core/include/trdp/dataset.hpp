#pragma once

#include "logging.hpp"
#include "types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace trdp
{

struct DatasetElementDef
{
    std::string name;
    TrdpType type{TrdpType::UINT8};
    std::size_t offset{0};
    std::size_t length{0};
    std::size_t arrayLength{1};
    std::size_t bitOffset{0};
};

struct DatasetDef
{
    uint16_t datasetId{0};
    std::string name;
    std::vector<DatasetElementDef> elements;

    std::size_t payloadSize() const;
    const DatasetElementDef *find(const std::string &elementName) const;
};

struct ElementValue
{
    DatasetElementDef element;
    std::vector<uint8_t> rawValue;
};

using ElementValues = std::vector<ElementValue>;

// Helpers
std::size_t expectedSize(const DatasetElementDef &def);
bool assignValue(ElementValue &value, const std::string &input);
bool packDatasetToPayload(const DatasetDef &dataset, const ElementValues &values, std::vector<uint8_t> &outBuffer);
bool unpackPayloadToDataset(const DatasetDef &dataset, const std::vector<uint8_t> &payload, ElementValues &outValues);

class DatasetRegistry
{
public:
    void add(DatasetDef def);
    const DatasetDef *find(uint16_t datasetId) const;
    std::vector<DatasetDef> list() const;

private:
    std::unordered_map<uint16_t, DatasetDef> datasets_;
};

} // namespace trdp

