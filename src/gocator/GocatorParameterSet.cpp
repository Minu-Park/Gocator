#include "gocator/GocatorParameterSet.h"

namespace gocator
{

GocatorParameterSet::GocatorParameterSet(const GoPxLSdk::GoJson& json)
    : data_(json)
{
}

void GocatorParameterSet::addParameter(const std::string& path, const GoPxLSdk::GoJson& value)
{
    data_.Set(path, value);
}

GoPxLSdk::GoJson GocatorParameterSet::getParameter(const std::string& path) const
{
    return data_.At(path);
}

bool GocatorParameterSet::hasParameter(const std::string& path) const
{
    try
    {
        data_.At(path);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

GoPxLSdk::GoJson GocatorParameterSet::toJson() const
{
    return data_;
}

void GocatorParameterSet::fromJson(const GoPxLSdk::GoJson& json)
{
    data_ = json;
}

std::map<std::string, std::string> GocatorParameterSet::flatten() const
{
    std::map<std::string, std::string> result;
    
    GoPxLSdk::GoJson flattened = data_;
    flattened.Flatten();
    
    for (auto it = flattened.Begin(); it != flattened.End(); it++)
    {
        result[it.Key()] = it.Value().ToString();
    }
    
    return result;
}

} // namespace gocator
