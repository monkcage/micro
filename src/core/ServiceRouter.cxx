
#include <mutex>
#include "ServiceRouter.hpp"



namespace easy {


struct ServiceInfo {
    std::string name;
    std::string ip;
    uint8_t     status;
};


ServiceRouter::ServiceRouter()
{
}

/*
* @descript: 注册服务
* @param - identity: zeromq用于标识连接的连接标识
*          fd      : 原生socket fd
*          servName: 服务名称
*/
void ServiceRouter::RegisterService(uint32_t identity, int fd, char const* servName)
{
    std::lock_guard<SpinLock> guard(lock_);
    auto item = router_.find(servName);
    if(item != router_.end()) {
        std::shared_ptr<ServiceGroup> group = item->second;
        group->sid.emplace_back(identity);
        group->sfd.emplace_back(fd);
        group->size++;
    }else{
        std::shared_ptr<ServiceGroup> group = std::make_shared<ServiceGroup>();
        group->sid.emplace_back(identity);
        group->sfd.emplace_back(fd);
        router_.emplace(servName, group);
        group->size++;
    }
    
}


int64_t ServiceRouter::FindService(char const* servName)
{
    std::lock_guard<SpinLock> guard(lock_);
    auto item = router_.find(servName);
    if(item != router_.end()) {
        std::shared_ptr<ServiceGroup> group = item->second;
        return group->sid[group->current++ % group->sid.size()];
    }
    return -1;
}


void ServiceRouter::GetServices(std::vector<ServiceInfo>& servs)
{

}


void ServiceRouter::RemoveService(char const* servName, uint32_t identity)
{
    std::lock_guard<SpinLock> guard(lock_);
    auto item = router_.find(servName);
    if(item != router_.end()) {
        std::shared_ptr<ServiceGroup> group = item->second;
        auto sid = group->sid;
        auto sfd = group->sfd;
        uint32_t idx = 0;
        uint32_t size = group->size;
        for( ; idx < size; ++idx) {
            if(sid[idx] == identity) {
                sid[idx] = sid[size-1];
                sfd[idx] = sfd[size-1];
                group->size--;
            }
        }
    }
}


void ServiceRouter::RemoveService(char const* servName, int fd)
{
    std::lock_guard<SpinLock> guard(lock_);
    auto item = router_.find(servName);
    if(item != router_.end()) {
        std::shared_ptr<ServiceGroup> group = item->second;
        auto sid = group->sid;
        auto sfd = group->sfd;
        uint32_t idx = 0;
        uint32_t size = group->size;
        for( ; idx < size; ++idx) {
            if(sfd[idx] == fd) {
                sid[idx] = sid[size-1];
                sfd[idx] = sfd[size-1];
                group->size--;
            }
        }
    }
}


}