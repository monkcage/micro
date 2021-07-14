#ifndef EASY_SERVICE_COUNTER_INC
#define EASY_SERVICE_COUNTER_INC

#include <memory>
#include "pugi/pugixml.hpp"

namespace easy {

class ServiceRouter;

class ServiceCounter 
{
public:
    ServiceCounter(char const* confile);

    void Start();

private:
    void startProxyThread();
    void startMonitorThread();
    void startHeartbeat();

private:
    void* ctx_;
    void* frontend_; // frontend用于接收REQUEST,并转发backend的RESPONSE
    void* backend_;  // backend用于 转发fronted的REQUEST,接收RESPONSE
    void* configCenter_; // ServiceCounter作为一个服务注册到配置中心
    std::shared_ptr<ServiceRouter> router_;
    char const* confile_;
    pugi::xml_node xmlconf_;
    pugi::xml_document xmldoc_;
};


}

#endif