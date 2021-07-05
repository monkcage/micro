#ifndef EASY_SERVICE_ROUTER_INC
#define EASY_SERVICE_ROUTER_INC

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

#include "zmq/zmq.h"

namespace easy {


class ServiceRouter 
{
    struct ServiceGroup {
        std::vector<uint32_t> sid;
        uint64_t              current;

        ServiceGroup()
            : current(0)
        {
        }
    };


    using ServiceMap = std::unordered_map<std::string, std::shared_ptr<ServiceGroup>>;
public:
    ServiceRouter();

    void RegisterService(uint32_t identity, char const* servName);

    int64_t FindService(char const* servName);

private:
    ServiceMap router_; 
};


}  // namespace easy


#endif 