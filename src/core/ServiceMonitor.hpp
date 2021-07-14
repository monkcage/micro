/**************************************************************************\
* @descript: 服务监控程序,从router中获取服务列表，并且通过zmq_socket_monitor监
*            控服务是否下线；并开启一个服务查询接口，对外提供查询服务
* @author  : monkcag3
* @created : 20210711-20:30:00
\**************************************************************************/
#ifndef EASY_CORE_SERVICE_MONITOR_INC
#define EASY_CORE_SERVICE_MONTTOR_INC

#include <memory>


namespace easy {

class ServiceRouter;

class ServiceMonitor 
{
public:
    ServiceMonitor(char const* apiUrl, char const* monitorUrl);

    void Start();

private:
    void processMonitorMessage(char const* data, uint32_t len);
    void processApiMessage(char const* data, uint32_t len);

private:
    std::shared_ptr<ServiceRouter> router_;
    void* ctx_;
    void* monitor_;
    void* api_;
    uint8_t const* apiUrl_;
    uint8_t const* monitorUrl_;
}


}


#endif