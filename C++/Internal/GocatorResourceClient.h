#pragma once

#include <string>

#include <GoPxLSdk/GoJson.h>

#include "Internal/GocatorConnection.h"

namespace gocator
{

class GocatorResourceClient
{
public:
    explicit GocatorResourceClient(GocatorConnection& connection);

    GoPxLSdk::GoJson read(const std::string& path);
    void update(const std::string& path, const GoPxLSdk::GoJson& payload);
    void call(const std::string& path, const GoPxLSdk::GoJson& payload);

    GoPxLSdk::GoJson schema(const std::string& path);
    GoPxLSdk::GoJson data(const std::string& path);
    void setJson(const std::string& path, const GoPxLSdk::GoJson& patch);

private:
    GocatorConnection& connection_;
};

} // namespace gocator
