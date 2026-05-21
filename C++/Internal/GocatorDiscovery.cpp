#include "Internal/GocatorDiscovery.h"

#include <stdexcept>
#include <utility>

#include <GoPxLSdk/GoDiscoveryClient.h>
#include <kApi/Io/kNetwork.h>

#include "Internal/GocatorSdkRuntime.h"

namespace gocator
{
namespace
{

std::string ipAddressToString(const kIpAddress& address)
{
    kChar text[64] = {};
    kIpAddress_Format(address, text, sizeof(text));
    return text;
}

void validateAddress(const std::string& address)
{
    kIpAddress parsed = {};
    const kStatus status = kIpAddress_Parse(&parsed, address.c_str());
    if (status != kOK)
    {
        throw std::invalid_argument("Invalid Gocator IP address: " + address);
    }
}

GocatorDeviceInfo toDeviceInfo(const GoPxLSdk::GoInstance& instance)
{
    GocatorDeviceInfo info;
    info.address = ipAddressToString(instance.GetIpAddress());
    info.appId = instance.GetAppId();
    info.appName = instance.GetAppName();
    info.appVersion = instance.GetAppVersion();
    info.deviceModel = instance.GetDeviceModel();
    info.gateway = ipAddressToString(instance.GetGateway());
    info.mask = ipAddressToString(instance.GetMask());
    info.controlPort = instance.GetControlPort();
    info.gdpPort = instance.GetGdpPort();
    info.webPort = instance.GetWebPort();
    info.serialNumber = instance.GetSerialNumber();
    info.dhcp = instance.GetIsDhcp();
    info.addressConflict = instance.GetIsAddressConflict();
    info.remote = instance.GetIsRemote();
    info.hmiStatus = instance.GetHMIStatus();
    return info;
}

} // namespace

GocatorConnectionConfig GocatorDeviceInfo::connectionConfig(int commandTimeoutMs) const
{
    return {address, controlPort, commandTimeoutMs};
}

bool GocatorDeviceInfo::canConnectLocally() const
{
    return !address.empty() && !addressConflict && !remote;
}

std::vector<GocatorDeviceInfo> GocatorDiscovery::discover(const GocatorDiscoveryOptions& options) const
{
    GocatorSdkRuntime::ensureInitialized();

    GoPxLSdk::GoDiscoveryClient discovery;
    discovery.BlockingDiscover(options.timeoutMs, options.classicDiscover);

    std::vector<GocatorDeviceInfo> devices;
    const std::vector<GoPxLSdk::GoInstance>& instances = discovery.InstanceList();
    devices.reserve(instances.size());

    for (const GoPxLSdk::GoInstance& instance : instances)
    {
        devices.push_back(toDeviceInfo(instance));
    }

    return devices;
}

GocatorConnectionConfig GocatorDiscovery::manualTarget(
    std::string address,
    std::uint16_t controlPort,
    int commandTimeoutMs)
{
    validateAddress(address);
    return {std::move(address), controlPort, commandTimeoutMs};
}

} // namespace gocator
