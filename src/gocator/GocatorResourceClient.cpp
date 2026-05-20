#include "gocator/GocatorResourceClient.h"

#include <GoPxLSdk/GoResource.h>
#include <GoPxLSdk/GoSystem.h>

namespace gocator
{

GocatorResourceClient::GocatorResourceClient(GocatorConnection& connection)
    : connection_(connection)
{
}

GoPxLSdk::GoJson GocatorResourceClient::read(const std::string& path)
{
    return connection_.system().Client().Read(path).GetResponse(connection_.commandTimeoutMs()).Payload();
}

void GocatorResourceClient::update(const std::string& path, const GoPxLSdk::GoJson& payload)
{
    connection_.system().Client().Update(path, payload).CheckResponse(connection_.commandTimeoutMs());
}

void GocatorResourceClient::call(const std::string& path, const GoPxLSdk::GoJson& payload)
{
    connection_.system().Client().Call(path, payload).CheckResponse(connection_.commandTimeoutMs());
}

GoPxLSdk::GoJson GocatorResourceClient::schema(const std::string& path)
{
    return connection_.system().Resource(path)->Schema();
}

} // namespace gocator
