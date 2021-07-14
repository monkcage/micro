
#include <fstream>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "log/easylogging++.h"
#include "ServiceRouter.hpp"
#include "ServiceCounter.hpp"


#include "nlohmann/json.hpp"


namespace easy {


using json = nlohmann::json;

namespace detail{

} // namespace detail




ServiceCounter::ServiceCounter(char const* confile)
    : router_(std::make_shared<ServiceRouter>())
    , confile_(confile)
{
    // json json_conf;
    // std::ifstream infile(confile);
    // infile >> json_conf;
    // JCONF = json_conf.get<detail::service_conf_t>();
    pugi::xml_parse_result result = xmldoc_.load_file(confile);
    pugi::xml_node root = xmldoc_.child("application");
    xmlconf_ = root.child("gateway");


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
    zmq_bind(frontend_, xmlconf_.child("frontend").attribute("value").as_string());
    zmq_bind(backend_, xmlconf_.child("backend").attribute("value").as_string());
    zmq_socket_monitor(backend_, xmlconf_.child("monitor").attribute("value").as_string(), ZMQ_EVENT_ALL);
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
                uint32_t id = *(uint32_t*)((char*)zmq_msg_data(&identity) + 1); // zeromq内置id
                // char const* address = zmq_msg_gets(&identity, "Peer-Address");
                // char const* test = zmq_msg_gets(&identity, "Socket-Type");
                // LOG(DEBUG) << " >>>> Address: " << address;
                // LOG(DEBUG) << " >>>> Socket Type: " << type;
                int fd = zmq_msg_get(&identity, ZMQ_SRCFD);
                // sockaddr_in addr;
                // socklen_t socklen = sizeof(addr);
                // getpeername(fd, (sockaddr*)&addr, &socklen);
                // LOG(DEBUG) << " >>>> Address: " << inet_ntoa(addr.sin_addr) << ":" << addr.sin_port;

                LOG(DEBUG) << "register service: conn(" << id << ") - " << (char*)(zmq_msg_data(&service)); 
                router_->RegisterService(id, fd, (char*)(zmq_msg_data(&service)));
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
    zmq_connect(sock, xmlconf_.child("monitor").attribute("value").as_string());

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


void ServiceCounter::startHeartbeat()
{
    configCenter_ = zmq_socket(ctx_, ZMQ_PUB);
    
}


} // namespace easy