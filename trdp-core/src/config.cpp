#include "trdp/config.hpp"

#include "trdp/logging.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace trdp
{

static DatasetDef makeExampleDataset()
{
    DatasetDef def;
    def.datasetId = 1;
    def.name = "ExampleDataset";
    def.elements.push_back({"counter", TrdpType::UINT32, 0, sizeof(uint32_t), 1, 0});
    def.elements.push_back({"text", TrdpType::STRING, 4, 16, 1, 0});
    return def;
}

std::optional<TrdpConfig> XmlConfigLoader::loadFromDeviceConfig(const std::string &deviceFile,
                                                                const std::string &comIdFile,
                                                                const std::string &pdFile,
                                                                const std::string &mdFile) const
{
    (void)deviceFile;
    (void)comIdFile;
    (void)pdFile;
    (void)mdFile;

    TrdpConfig config;
    // Placeholder: provide an example dataset/PD/MD so the simulator runs without XML.
    config.datasetRegistry.add(makeExampleDataset());

    PdPublishTelegram pub;
    pub.comId = 1001;
    pub.datasetId = 1;
    pub.destinationIp = "239.1.1.1";
    pub.cycleTimeMs = 1000;
    pub.values = {ElementValue{*config.datasetRegistry.find(1)->find("counter"), {0, 0, 0, 0}},
                  ElementValue{*config.datasetRegistry.find(1)->find("text"), std::vector<uint8_t>(16, 0)}};
    config.pdPublish.push_back(pub);

    PdSubscribeTelegram sub;
    sub.comId = 1001;
    sub.datasetId = 1;
    sub.sourceIp = "0.0.0.0";
    sub.destinationIp = "239.1.1.1";
    sub.timeoutMs = 2000;
    config.pdSubscribe.push_back(sub);

    MdTemplate req;
    req.name = "example-request";
    req.direction = MdDirection::Request;
    req.comId = 2001;
    req.datasetId = 1;
    req.destinationIp = "127.0.0.1";
    req.destinationPort = 17225;
    req.values = pub.values;
    config.mdTemplates.push_back(req);

    info("Loaded stub configuration with one dataset, PD publish/subscription, and MD template.");
    return config;
}

} // namespace trdp

