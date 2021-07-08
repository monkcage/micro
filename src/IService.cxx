
#include <iostream>
#include <vector>
#include <fstream>
#include <thread>

#include "log/easylogging++.h"
#include "nlohmann/json.hpp"
#include "IService.hpp"


namespace easy {


using json = nlohmann::json;

static char const* URI_FRONTEND_MONITOR = "inproc://easy.frontend.monitor";
static char const* URI_PROXY_ROUTER     = "inproc://easy.proxy.router";
static char const* URI_PROXY_JOINT      = "inproc://easy.proxy.joint";


namespace detail{

enum conn_status {
    NotConnected = 0x00,
    Connecting,
    Connected,
    Disconnected
};

struct service_conf_t {
    std::string              service;
    std::string              backend;
    std::vector<std::string> gateways;
};

void from_json(json const& j, service_conf_t& conf)
{
    j.at("service").get_to(conf.service);
    j.at("backend").get_to(conf.backend);
    j.at("gateways").get_to(conf.gateways);
}


struct sock_t {
    void*       zsock;
    uint32_t    id;
    bool        active;
    conn_status status;
};


} // namespace detail


static detail::service_conf_t JCONF;



IService::IService(char const* confile)
    : index_(0)
    , stoped_(false)
{
    json json_conf;
    std::ifstream infile(confile);
    infile >> json_conf;
    JCONF = json_conf.get<detail::service_conf_t>();

    ctx_ = zmq_ctx_new();
}


IService::~IService()
{
    for(auto&& thr: threads_) {
        if(thr.joinable()) 
            thr.join();
    }
}


void IService::Start()
{
    for(uint32_t idx = 0; idx < JCONF.gateways.size(); ++idx) {
        detail::sock_t* sock = new detail::sock_t;
        sock->zsock = zmq_socket(ctx_, ZMQ_DEALER);
        sock->id = idx;
        sock->status = NotConnected;
        // 先开启监控线程,再进行连接
        threads_.emplace_back(std::thread(std::bind(&IService::startMonitorThread, this, idx)));
        zmq_connect(sock->zsock, JCONF.gateways[idx].c_str());
        proxyDealers_.emplace_back(sock);
    }
    proxyJoint_ = zmq_socket(ctx_, ZMQ_DEALER);
    proxyRouter_ = zmq_socket(ctx_, ZMQ_ROUTER);
    proxyDealer_ = zmq_socket(ctx_, ZMQ_DEALER);
    zmq_bind(proxyRouter_, URI_PROXY_ROUTER);
    zmq_bind(proxyDealer_, JCONF.backend.c_str());
    zmq_connect(proxyJoint_, URI_PROXY_JOINT);

    threads_.emplace_back(std::thread(std::bind(&IService::startFrontendThread, this)));
    threads_.emplace_back(std::thread(std::bind(&IService::startBackendThread, this)));
}


void IService::startBackendThread()
{
    zmq_proxy(proxyRouter_, proxyDealer_, nullptr);
}


void IService::startFrontendThread()
{
    // first: register to gateway
    // send service name to gateway
    for(auto&& item: proxyDealers_) {
        zmq_msg_t identity;  zmq_msg_init_size(&identity, JCONF.service.size() + 1);
        memcpy(zmq_msg_data(&identity), JCONF.service.c_str(), JCONF.service.size() + 1);
        int rc = zmq_msg_send(&identity, ((detail::sock_t*)item)->zsock, 0);
        LOG(DEBUG) << "Send service name to Gateway ret: " << rc;
    }

    // second: poll in/out
    zmq_pollitem_t* items = new zmq_pollitem_t[proxyDealers_.size() + 1];
    uint32_t i;
    for(i=0; i<proxyDealers_.size(); ++i) {
        items[i] = {((detail::sock_t*)(proxyDealers_[i]))->zsock, 0, ZMQ_POLLIN, 0};
    }
    items[i] = {proxyJoint_, 0, ZMQ_POLLIN, 0};
    while(!stoped_) {
        zmq_poll(items, proxyDealers_.size(), -1);
        int more = 0;
        size_t more_size = sizeof(more);
        for(i=0; i<proxyDealers_.size(); ++i) {
            if(items[i].revents & ZMQ_POLLIN) {
                // TODO: send to router by proxyJoint;
                // 第一帧加上sock_t id
                zmq_msg_t identity; zmq_msg_init_size(&identity, sizeof(uint32_t));
                *(uint32_t*)zmq_msg_data(&identity) = ((detail::sock_t*)(&(items[i].socket)))->id;
                
                // zmq_msg_send(&identity, proxyJoint_, ZMQ_SNDMORE);
                zmq_getsockopt(items[i].socket, ZMQ_RCVMORE, &more, &more_size);
                while(more != 0) {
                    zmq_msg_t content;
                    zmq_msg_init(&content);
                    zmq_msg_recv(&content, items[i].socket, 0);
                    // zmq_msg_send(&content, proxyJoint_, 0);
                    zmq_msg_close(&content);
                    zmq_getsockopt(items[i].socket, ZMQ_RCVMORE, &more, &more_size);
                }
            }
        }
        if(items[i].revents & ZMQ_POLLIN) {
            // TODO: recv data from proxyJoint, and send by sock id;
            // 
        }
    } 
}

void IService::startMonitorThread(uint32_t idx)
{

}

void IService::Stop()
{

}



} // namepsace easy



