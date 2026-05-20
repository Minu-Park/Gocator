#pragma once

#include <string>

#include <GoPxLSdk/GoJson.h>

#include "gocator/GocatorConnection.h"

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

private:
    GocatorConnection& connection_;
};

} // namespace gocator
