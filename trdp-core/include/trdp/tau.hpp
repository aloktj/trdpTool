#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#ifdef TRDP_AVAILABLE
extern "C" {
#include <tau_marshall.h>
#include <tau_xml.h>
}
#endif

namespace trdp
{

class TauMarshaller
{
public:
    TauMarshaller();
    ~TauMarshaller();

    TauMarshaller(const TauMarshaller &) = delete;
    TauMarshaller &operator=(const TauMarshaller &) = delete;

    TauMarshaller(TauMarshaller &&) noexcept;
    TauMarshaller &operator=(TauMarshaller &&) noexcept;

    /**
     * Initialise marshalling support from a standard TRDP device XML file.
     * When TRDP is not available at build time this will return nullptr.
     */
    static std::unique_ptr<TauMarshaller> createFromXml(const std::string &deviceFile);

    /**
     * Convert host-order payload to the TRDP network representation for the given ComId.
     * Returns false if no marshalling context is available or the ComId is unknown.
     */
    bool marshall(uint32_t comId, const std::vector<uint8_t> &hostPayload, std::vector<uint8_t> &networkPayload) const;

    /**
     * Convert TRDP network payload back into host byte order for the given ComId.
     */
    bool unmarshall(uint32_t comId, const std::vector<uint8_t> &networkPayload, std::vector<uint8_t> &hostPayload) const;

    /**
     * Lookup the dataset id associated with a ComId from the XML-derived map.
     */
    uint16_t datasetForComId(uint32_t comId) const;

    bool valid() const;

private:
#ifdef TRDP_AVAILABLE
    void release();

    void *context_{nullptr};
    uint32_t numComIds_{0};
    uint32_t numDatasets_{0};
    TRDP_COMID_DSID_MAP_T *comIdMapRaw_{nullptr};
    apTRDP_DATASET_T datasetsRaw_{nullptr};
    std::vector<TRDP_COMID_DSID_MAP_T> comIdMap_;
#endif
};

} // namespace trdp

