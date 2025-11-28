#pragma once

#include "config.hpp"

#include <functional>
#include <string>

namespace trdp
{

class MdEngine
{
public:
    explicit MdEngine(TrdpConfig &config);

    void listTemplates(std::ostream &os) const;
    bool setTemplateValue(const std::string &name, const std::string &element, const std::string &value);
    bool clearTemplate(const std::string &name);
    bool sendTemplate(const std::string &name, std::ostream &os) const;

private:
    TrdpConfig &config_;
};

} // namespace trdp

