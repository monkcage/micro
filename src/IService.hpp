#ifndef EASY_CORE_SERVICE_INC
#define EASY_CORE_SERVICE_INC

#include <vector>
#include <thread>
#include "zmq/zmq.h"

namespace easy {


class IService {
public:
    IService(char const* confile);
    virtual ~IService();

    void Start();
    void Stop();

private:
    void startBackendThread();
    void startFrontendThread(); 
    void startMonitorThread(uint32_t idx);

private:
    void* ctx_;
    std::vector<void*> proxyDealers_;
    uint32_t           index_;
    void* proxyJoint_;
    void* proxyRouter_;
    void* proxyDealer_;
    bool  stoped_;
    std::vector<std::thread> threads_;
};


}



#endif 