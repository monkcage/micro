
#include "ServiceRouter.hpp"


namespace easy {


ServiceRouter::ServiceRouter()
{
}


void ServiceRouter::RegisterService(uint32_t identity, char const* servName)
{
    auto item = router_.find(servName);
    if(item != router_.end()) {
        std::shared_ptr<ServiceGroup> group = item->second;
        group->sid.emplace_back(identity);
    }else{
        std::shared_ptr<ServiceGroup> group = std::make_shared<ServiceGroup>();
        router_.emplace(servName, group);
    }
}


int64_t ServiceRouter::FindService(char const* servName)
{
    auto item = router_.find(servName);
    if(item != router_.end()) {
        std::shared_ptr<ServiceGroup> group = item->second;
        return group->sid[group->current++ % group->sid.size()];
    }
    return -1;
}


}