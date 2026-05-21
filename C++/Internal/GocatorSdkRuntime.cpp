#include "Internal/GocatorSdkRuntime.h"

#include <stdexcept>

#include <GoApi/GoApiLib.h>

namespace gocator
{
namespace
{

class GoApiRuntime
{
public:
    GoApiRuntime()
    {
        const kStatus status = GoApiLib_Construct(&assembly_);
        if (status != kOK)
        {
            throw std::runtime_error("GoApiLib_Construct failed");
        }
    }

    ~GoApiRuntime()
    {
        kDestroyRef(&assembly_);
    }

private:
    kAssembly assembly_ = kNULL;
};

} // namespace

void GocatorSdkRuntime::ensureInitialized()
{
    static GoApiRuntime runtime;
    (void)runtime;
}

} // namespace gocator
