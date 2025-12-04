#include "trdp/tau.hpp"

#include "trdp/logging.hpp"

#include <algorithm>

namespace trdp
{

TauMarshaller::TauMarshaller() = default;
TauMarshaller::~TauMarshaller()
{
#ifdef TRDP_AVAILABLE
    release();
#endif
}

TauMarshaller::TauMarshaller(TauMarshaller &&other) noexcept
{
#ifdef TRDP_AVAILABLE
    context_ = other.context_;
    numComIds_ = other.numComIds_;
    numDatasets_ = other.numDatasets_;
    comIdMapRaw_ = other.comIdMapRaw_;
    datasetsRaw_ = other.datasetsRaw_;
    comIdMap_ = std::move(other.comIdMap_);

    other.context_ = nullptr;
    other.comIdMapRaw_ = nullptr;
    other.datasetsRaw_ = nullptr;
    other.numComIds_ = 0u;
    other.numDatasets_ = 0u;
#else
    (void)other;
#endif
}

TauMarshaller &TauMarshaller::operator=(TauMarshaller &&other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
#ifdef TRDP_AVAILABLE
    release();

    context_ = other.context_;
    numComIds_ = other.numComIds_;
    numDatasets_ = other.numDatasets_;
    comIdMapRaw_ = other.comIdMapRaw_;
    datasetsRaw_ = other.datasetsRaw_;
    comIdMap_ = std::move(other.comIdMap_);

    other.context_ = nullptr;
    other.comIdMapRaw_ = nullptr;
    other.datasetsRaw_ = nullptr;
    other.numComIds_ = 0u;
    other.numDatasets_ = 0u;
#else
    (void)other;
#endif
    return *this;
}

std::unique_ptr<TauMarshaller> TauMarshaller::createFromXml(const std::string &deviceFile)
{
#ifdef TRDP_AVAILABLE
    TRDP_XML_DOC_HANDLE_T doc{};
    const auto prepErr = tau_prepareXmlDoc(deviceFile.c_str(), &doc);
    if (prepErr != TRDP_NO_ERR)
    {
        warn("tau_prepareXmlDoc failed for " + deviceFile + " (" + std::to_string(prepErr) + ")");
        return nullptr;
    }

    auto marshaller = std::unique_ptr<TauMarshaller>(new TauMarshaller());
    TRDP_COMID_DSID_MAP_T *comIdMap = nullptr;
    apTRDP_DATASET_T datasets = nullptr;

    const auto readErr = tau_readXmlDatasetConfig(&doc, &marshaller->numComIds_, &comIdMap, &marshaller->numDatasets_, &datasets);
    tau_freeXmlDoc(&doc);
    if (readErr != TRDP_NO_ERR)
    {
        warn("tau_readXmlDatasetConfig failed (" + std::to_string(readErr) + ")");
        return nullptr;
    }

    void *context = nullptr;
    const auto initErr = tau_initMarshall(&context, marshaller->numComIds_, comIdMap, marshaller->numDatasets_, datasets);
    if (initErr != TRDP_NO_ERR)
    {
        warn("tau_initMarshall failed (" + std::to_string(initErr) + ")");
        tau_freeXmlDatasetConfig(marshaller->numComIds_, comIdMap, marshaller->numDatasets_, datasets);
        return nullptr;
    }

    marshaller->context_ = context;
    marshaller->comIdMapRaw_ = comIdMap;
    marshaller->datasetsRaw_ = datasets;
    marshaller->comIdMap_.assign(comIdMap, comIdMap + marshaller->numComIds_);

    info("tau_marshall initialised for " + std::to_string(marshaller->numComIds_) + " ComIds from XML");
    return marshaller;
#else
    (void)deviceFile;
    return nullptr;
#endif
}

bool TauMarshaller::valid() const
{
#ifdef TRDP_AVAILABLE
    return context_ != nullptr;
#else
    return false;
#endif
}

bool TauMarshaller::marshall(uint32_t comId, const std::vector<uint8_t> &hostPayload, std::vector<uint8_t> &networkPayload) const
{
#ifdef TRDP_AVAILABLE
    if (!valid())
    {
        warn("tau_marshall called without initialised context");
        return false;
    }

    if (datasetForComId(comId) == 0u)
    {
        warn("tau_marshall: unknown ComId " + std::to_string(comId));
        return false;
    }

    UINT32 destSize = static_cast<UINT32>(hostPayload.size());
    networkPayload.resize(hostPayload.size());
    TRDP_DATASET_T *cached = nullptr;
    const auto err = tau_marshall(context_, comId, hostPayload.data(), static_cast<UINT32>(hostPayload.size()),
                                  networkPayload.data(), &destSize, &cached);
    if (err != TRDP_NO_ERR)
    {
        warn("tau_marshall failed for ComId " + std::to_string(comId) + " (" + std::to_string(err) + ")");
        return false;
    }
    networkPayload.resize(destSize);
    return true;
#else
    (void)comId;
    networkPayload = hostPayload;
    return false;
#endif
}

bool TauMarshaller::unmarshall(uint32_t comId, const std::vector<uint8_t> &networkPayload, std::vector<uint8_t> &hostPayload) const
{
#ifdef TRDP_AVAILABLE
    if (!valid())
    {
        warn("tau_unmarshall called without initialised context");
        return false;
    }

    UINT32 destSize = static_cast<UINT32>(networkPayload.size());
    hostPayload.resize(networkPayload.size());
    TRDP_DATASET_T *cached = nullptr;
    const auto err = tau_unmarshall(context_, comId, const_cast<uint8_t *>(networkPayload.data()),
                                    static_cast<UINT32>(networkPayload.size()), hostPayload.data(), &destSize, &cached);
    if (err != TRDP_NO_ERR)
    {
        warn("tau_unmarshall failed for ComId " + std::to_string(comId) + " (" + std::to_string(err) + ")");
        return false;
    }
    hostPayload.resize(destSize);
    return true;
#else
    (void)comId;
    hostPayload = networkPayload;
    return false;
#endif
}

uint16_t TauMarshaller::datasetForComId(uint32_t comId) const
{
#ifdef TRDP_AVAILABLE
    const auto it = std::find_if(comIdMap_.begin(), comIdMap_.end(),
                                 [&](const auto &entry) { return entry.comId == comId; });
    if (it == comIdMap_.end())
    {
        return 0u;
    }
    return static_cast<uint16_t>(it->datasetId);
#else
    (void)comId;
    return 0u;
#endif
}

#ifdef TRDP_AVAILABLE
void TauMarshaller::release()
{
    if (comIdMapRaw_ != nullptr || datasetsRaw_ != nullptr)
    {
        tau_freeXmlDatasetConfig(numComIds_, comIdMapRaw_, numDatasets_, datasetsRaw_);
    }

    context_ = nullptr;
    numComIds_ = 0u;
    numDatasets_ = 0u;
    comIdMapRaw_ = nullptr;
    datasetsRaw_ = nullptr;
    comIdMap_.clear();
}
#endif

} // namespace trdp

