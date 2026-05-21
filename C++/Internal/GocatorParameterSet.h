#pragma once

#include <string>
#include <vector>
#include <map>
#include <GoPxLSdk/GoJson.h>

namespace gocator
{

/**
 * Represents a set of Gocator parameters.
 * Can be used to store, transfer, and apply configuration.
 */
class GocatorParameterSet
{
public:
    GocatorParameterSet() = default;
    explicit GocatorParameterSet(const GoPxLSdk::GoJson& json);

    void addParameter(const std::string& path, const GoPxLSdk::GoJson& value);
    GoPxLSdk::GoJson getParameter(const std::string& path) const;
    bool hasParameter(const std::string& path) const;

    GoPxLSdk::GoJson toJson() const;
    void fromJson(const GoPxLSdk::GoJson& json);

    /**
     * Flattens the parameter set into a map of paths to values.
     * Useful for listing or searching parameters.
     */
    std::map<std::string, std::string> flatten() const;

private:
    GoPxLSdk::GoJson data_;
};

} // namespace gocator
