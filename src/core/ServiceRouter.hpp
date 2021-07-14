#ifndef EASY_SERVICE_ROUTER_INC
#define EASY_SERVICE_ROUTER_INC

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include "base/SpinLock.hpp"

#include "zmq/zmq.h"

namespace easy {


struct ServiceInfo;


class ServiceRouter 
{
    struct ServiceGroup {
        std::vector<uint32_t> sid;
        std::vector<uint32_t> sfd;
        // std::vector<uint32_t> sid;
        uint64_t              current;
        uint64_t              size;

        ServiceGroup()
            : current(0)
            , size(0)
        {
        }
    };


    using ServiceMap = std::unordered_map<std::string, std::shared_ptr<ServiceGroup>>;
public:
    ServiceRouter();
    void RegisterService(uint32_t identity, int fd, char const* servName);

    int64_t FindService(char const* servName);

    void RemoveService(char const* host);

    void GetServices(std::vector<ServiceInfo>& servs);

    void RemoveService(char const* servName, uint32_t identity);
    void RemoveService(char const* servName, int fd);

private:
    ServiceMap router_; 
    SpinLock  lock_;
};


}  // namespace easy


#endif 