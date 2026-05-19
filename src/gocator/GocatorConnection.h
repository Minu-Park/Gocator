#pragma once

#include <cstdint>
#include <memory>

#include "gocator/GocatorTypes.h"

namespace GoPxLSdk
{
class GoSystem;
}

namespace gocator
{

class GocatorConnection
{
public:
    explicit GocatorConnection(GocatorConnectionConfig config);
    ~GocatorConnection();

    GocatorConnection(const GocatorConnection&) = delete;
    GocatorConnection& operator=(const GocatorConnection&) = delete;

    const GocatorConnectionConfig& config() const noexcept;

    void connect();
    void disconnect() noexcept;
    bool isConnected();

    void start();
    void stop();
    void stopNoThrow() noexcept;

    std::uint16_t controlPort() const noexcept;
    std::uint16_t gdpPort();
    int commandTimeoutMs() const noexcept;

    GoPxLSdk::GoSystem& system();
    const GoPxLSdk::GoSystem& system() const;

private:
    GocatorConnectionConfig config_;
    std::unique_ptr<GoPxLSdk::GoSystem> system_;
};

} // namespace gocator
