
#include <fstream>
#include <thread>
#include "log/easylogging++.h"
#include "ServiceRouter.hpp"
#include "ServiceCounter.hpp"


#include "nlohmann/json.hpp"


namespace easy {


using json = nlohmann::json;

namespace detail{

enum conn_status {
    NotConnected = 0x00,
    Connecting,
    Connected,
    Disconnected
};

struct service_conf_t {
    std::string frontend;
    std::string backend;
    std::string monitor;
};

void from_json(json const& j, service_conf_t& conf)
{
    j.at("frontend").get_to(conf.frontend);
    j.at("backend").get_to(conf.backend);
    j.at("monitor").get_to(conf.monitor);
}

} // namespace detail


struct peer_t {
    void* zsock;
    int   id;
    int   index;
    int   fd;
}



static detail::service_conf_t JCONF;


ServiceCounter::ServiceCounter(char const* confile)
    : router_(std::make_shared<ServiceRouter>())
    , confile_(confile)
{
    json json_conf;
    std::ifstream infile(confile);
    infile >> json_conf;
    JCONF = json_conf.get<detail::service_conf_t>();

    ctx_ = zmq_ctx_new();
    frontend_ = zmq_socket(ctx_, ZMQ_ROUTER);
    backend_ = zmq_socket(ctx_, ZMQ_ROUTER);
    int linger = 1000;
    zmq_setsockopt(frontend_, ZMQ_LINGER, &linger, sizeof(linger));
    zmq_setsockopt(backend_, ZMQ_LINGER, &linger, sizeof(linger));
}


void ServiceCounter::Start()
{
    std::thread proxy([&](){
        startProxyThread();
    });
    std::thread monitor([&](){
        startMonitorThread();
    });

    proxy.join();
    monitor.join();
}



void ServiceCounter::startProxyThread()
{
    zmq_bind(frontend_, JCONF.frontend.c_str());
    zmq_bind(backend_, JCONF.backend.c_str());
    zmq_socket_monitor(backend_, JCONF.monitor.c_str(), ZMQ_EVENT_ALL);
    zmq_pollitem_t items[] = {
        {frontend_, 0, ZMQ_POLLIN, 0},
        {backend_, 0, ZMQ_POLLIN, 0}
    };
    int more = 0;
    size_t more_size = sizeof(more);
    while(true) {
        zmq_poll(items, 2, -1);
        if(items[0].revents & ZMQ_POLLIN) {
            zmq_msg_t nullframe; zmq_msg_init(&nullframe);
            zmq_msg_t identity;  zmq_msg_init(&identity);
            zmq_msg_t service;   zmq_msg_init(&service);
            zmq_msg_t content;   zmq_msg_init(&content);

            zmq_msg_recv(&identity, frontend_, 0);
            zmq_msg_recv(&nullframe, frontend_, 0);
            zmq_msg_recv(&service, frontend_, 0);
            zmq_msg_recv(&content, frontend_, 0);
            
            int64_t sid = router_->FindService((char*)(zmq_msg_data(&service)));
            zmq_msg_t sidentity; zmq_msg_init_size(&sidentity, 5);
            char* buff = (char*)(zmq_msg_data(&sidentity));
            *buff = 0;
            *(uint32_t*)(buff + 1) = sid;
            if(sid <= 0) {
                LOG(WARNING) << "Can not find service - " << (char*)zmq_msg_data(&service) << std::endl;
            }else{
                LOG(INFO) << "Find service: conn(" << sid << ") - " << (char*)zmq_msg_data(&service) << std::endl;
            }

            zmq_msg_send(&sidentity, backend_, ZMQ_SNDMORE);
            zmq_msg_send(&identity, backend_, ZMQ_SNDMORE);
            zmq_msg_send(&content, backend_, 0);
            // TODO: close message
            zmq_msg_close(&nullframe);
            zmq_msg_close(&identity);
            zmq_msg_close(&service);
            zmq_msg_close(&content);
        }
        if(items[1].revents & ZMQ_POLLIN) {
            zmq_msg_t nullframe; zmq_msg_init(&nullframe);
            zmq_msg_t identity;  zmq_msg_init(&identity);
            zmq_msg_t service;   zmq_msg_init(&service);
            zmq_msg_t content;   zmq_msg_init(&content);

            zmq_msg_recv(&identity, backend_, 0);
            LOG(DEBUG) << "RECV IDENTITY";
            zmq_msg_recv(&service, backend_, 0);
            LOG(DEBUG) << "RECV SERVICE: " << (char*)(zmq_msg_data(&service));
            zmq_getsockopt(backend_, ZMQ_RCVMORE, &more, &more_size);
            if(more != 0) {
                zmq_msg_recv(&content, backend_, 0);

                zmq_msg_send(&service, frontend_, ZMQ_SNDMORE);
                zmq_msg_send(&nullframe, frontend_, ZMQ_SNDMORE);
                zmq_msg_send(&content, frontend_, 0);
            }else{ // 服务注册只有两帧数据
                uint32_t id = *(uint32_t*)((char*)zmq_msg_data(&identity) + 1);
                LOG(INFO) << "register service: conn(" << id << ") - " << (char*)(zmq_msg_data(&service)); 
                router_->RegisterService(id, (char*)(zmq_msg_data(&service)));
                zmq_msg_t dummy; zmq_msg_init(&dummy);
                zmq_msg_send(&identity, backend_, ZMQ_SNDMORE);
                zmq_msg_send(&dummy, backend_, 0);
                zmq_msg_close(&dummy);
            }
            zmq_msg_close(&nullframe);
            zmq_msg_close(&identity);
            zmq_msg_close(&service);
            zmq_msg_close(&content);
        }
    }
}


void ServiceCounter::startMonitorThread()
{
    void* sock = zmq_socket(ctx_, ZMQ_PAIR);
    zmq_connect(sock, JCONF.monitor.c_str());

    zmq_pollitem_t items = {sock, 0, ZMQ_POLLIN, 0};
    while(true) {
        zmq_poll(&items, 1, -1);
        if(items.revents & ZMQ_POLLIN) {
            zmq_msg_t content; zmq_msg_init(&content);
            zmq_msg_t address; zmq_msg_init(&address);
            zmq_msg_recv(&content, sock, 0);
            zmq_msg_recv(&address, sock, 0);
            uint8_t const* buff = (uint8_t*)zmq_msg_data(&content);
            uint16_t event = *(uint16_t*)(buff);
            uint32_t value = *(uint32_t*)(buff + 2);
            if(ZMQ_EVENT_ACCEPTED == event) {
                LOG(DEBUG) << "Connected.";
            }else if(ZMQ_EVENT_DISCONNECTED == event) {
                LOG(DEBUG) << "Disconnected.";
            }
        }
    }
}


} // namespace easy