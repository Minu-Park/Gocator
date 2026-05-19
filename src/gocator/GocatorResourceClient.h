#pragma once

#include <string>
#include <vector>

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
    GoPxLSdk::GoJson schemaFor(const std::string& path, const std::string& propertyPath);
    std::vector<std::string> childUris(const std::string& path);
    std::vector<std::string> childUris(const std::string& path, const std::string& relationType);

private:
    GocatorConnection& connection_;
};

} // namespace gocator
