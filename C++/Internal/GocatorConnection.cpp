#include "Internal/GocatorConnection.h"

#include <stdexcept>
#include <utility>

#include <GoPxLSdk/GoSystem.h>
#include <kApi/Io/kNetwork.h>

#include "Internal/GocatorSdkRuntime.h"

namespace gocator
{

GocatorConnection::GocatorConnection(GocatorConnectionConfig config)
    : config_(std::move(config))
{
    GocatorSdkRuntime::ensureInitialized();

    kIpAddress address = {};
    const kStatus parseStatus = kIpAddress_Parse(&address, config_.address.c_str());
    if (parseStatus != kOK)
    {
        throw std::invalid_argument("Invalid Gocator IP address: " + config_.address);
    }

    system_ = std::make_unique<GoPxLSdk::GoSystem>(address, config_.controlPort);
}

GocatorConnection::~GocatorConnection()
{
    disconnect();
}

const GocatorConnectionConfig& GocatorConnection::config() const noexcept
{
    return config_;
}

void GocatorConnection::connect()
{
    system().Connect();
}

void GocatorConnection::disconnect() noexcept
{
    if (!system_)
    {
        return;
    }

    try
    {
        if (system_->IsConnected())
        {
            system_->Disconnect();
        }
    }
    catch (...)
    {
    }
}

bool GocatorConnection::isConnected()
{
    return system().IsConnected();
}

void GocatorConnection::start()
{
    system().Start();
}

void GocatorConnection::stop()
{
    system().Stop();
}

void GocatorConnection::stopNoThrow() noexcept
{
    try
    {
        stop();
    }
    catch (...)
    {
    }
}

std::uint16_t GocatorConnection::controlPort() const noexcept
{
    return config_.controlPort;
}

std::uint16_t GocatorConnection::gdpPort()
{
    return system().GdpPort();
}

int GocatorConnection::commandTimeoutMs() const noexcept
{
    return config_.commandTimeoutMs;
}

GoPxLSdk::GoSystem& GocatorConnection::system()
{
    if (!system_)
    {
        throw std::logic_error("Gocator system is not initialized");
    }

    return *system_;
}

const GoPxLSdk::GoSystem& GocatorConnection::system() const
{
    if (!system_)
    {
        throw std::logic_error("Gocator system is not initialized");
    }

    return *system_;
}

} // namespace gocator
