
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


void IService::Start(WorkProcessor const& processor)
{
    // 先开启后端处理线程:包括proxy线程和worker线程
    proxyRouter_ = zmq_socket(ctx_, ZMQ_ROUTER);
    proxyDealer_ = zmq_socket(ctx_, ZMQ_DEALER);
    zmq_bind(proxyRouter_, URI_PROXY_ROUTER);
    zmq_bind(proxyDealer_, JCONF.backend.c_str());
    threads_.emplace_back(std::thread(std::bind(&IService::startBackendProxyThread, this, proxyRouter_, proxyDealer_)));
    // 开启一个worker用于测试
    threads_.emplace_back(std::thread(std::bind(&IService::startWorkThread, this, processor)));
    // threads_.emplace_back(std::thread(std::bind(&IService::startFrontendThread, this)));
    // threads_.emplace_back(std::thread(std::bind(&IService::startBackendThread, this)));

    // Frontend初始化,服务注册
    int rc = 0;
    zmq_msg_t identity; zmq_msg_init_size(&identity, JCONF.service.size() + 1);
    memcpy(zmq_msg_data(&identity), JCONF.service.c_str(), JCONF.service.size() + 1);
    for(uint32_t idx = 0; idx < JCONF.gateways.size(); ++idx) {
        detail::sock_t* sock = new detail::sock_t;
        sock->zsock = zmq_socket(ctx_, ZMQ_DEALER);
        sock->id = idx;
        sock->status = detail::NotConnected;
        int timeout = 100;
        zmq_setsockopt(sock->zsock, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
        // 先开启监控线程,再进行连接
        //threads_.emplace_back(std::thread(std::bind(&IService::startMonitorThread, this, idx)));
        zmq_connect(sock->zsock, JCONF.gateways[idx].c_str());

        // 发送当前服务名称并等待网关确认
        zmq_msg_send(&identity, sock->zsock, 0);
        zmq_msg_t message; zmq_msg_init(&message);
        int more = 1;
        size_t more_size = sizeof(more);
        while(more != 0) {
            rc = zmq_msg_recv(&message, sock->zsock, 0);
            if(rc < 0) 
                break;
            zmq_getsockopt(sock->zsock, ZMQ_RCVMORE, &more, &more_size);  
        }
        if(rc >= 0) {
            void* backend = zmq_socket(ctx_, ZMQ_DEALER);
            zmq_connect(backend, URI_PROXY_ROUTER);
            threads_.emplace_back(std::thread(std::bind(&IService::startFrontendProxyThread, this, sock->zsock, backend)));
            proxyDealers_.emplace_back(sock);
        }else{
            LOG(WARNING) << "Can not connect to gateway(" << JCONF.gateways[idx] << ")";
        }
    }
    if(proxyDealers_.size() < 1) { // 确保至少连接到一个网关,否则抛出错误
        throw std::runtime_error("Cannot connect to any gateway");
    }
}


void IService::startBackendThread()
{
    zmq_proxy(proxyRouter_, proxyDealer_, nullptr);
}

void IService::startBackendProxyThread(void* frontend, void* backend)
{
    zmq_proxy(frontend, backend, nullptr);
}


void IService::startFrontendProxyThread(void* frontend, void* backend)
{
    zmq_pollitem_t items[2] = {
        {frontend, 0, ZMQ_POLLIN, 0},
        {backend, 0, ZMQ_POLLIN, 0}
    };
    int rc = 0;
    while(!stoped_) {
        rc = zmq_poll(items, 2, -1);
        if(items[0].revents & ZMQ_POLLIN) {
            // 接收两帧数据: 第一帧为客户端标识,第二帧为消息内容
            zmq_msg_t identity; zmq_msg_init(&identity);
            zmq_msg_t content; zmq_msg_init(&content);
            int more = 1;
            size_t more_size = sizeof(more);
            while(more != 0) {
                zmq_msg_recv(&identity, frontend, 0);
                LOG(DEBUG) << *(uint32_t*)((char*)(zmq_msg_data(&identity)+1));
                zmq_msg_recv(&content, frontend, 0);
                LOG(DEBUG)<< (char*)(zmq_msg_data(&content));
                zmq_msg_send(&identity, backend, 0);
                zmq_msg_send(&content, backend, 0);
                zmq_getsockopt(frontend, ZMQ_RCVMORE, &more, &more_size);
            }
            zmq_msg_close(&identity);
            zmq_msg_close(&content);
        }
        if(items[1].revents & ZMQ_POLLIN) {
            zmq_msg_t identity; zmq_msg_init(&identity);
            zmq_msg_t content; zmq_msg_init(&content);
            int more = 1;
            size_t more_size = sizeof(more);
            while(more != 0) {
                zmq_msg_recv(&identity, backend, 0);
                zmq_msg_recv(&content, backend, 0);
                zmq_msg_send(&identity, frontend, 0);
                zmq_msg_send(&content, frontend, 0);
                zmq_getsockopt(&backend, ZMQ_RCVMORE, &more, &more_size);
            }
            zmq_msg_close(&identity);
            zmq_msg_close(&content);
        }
    }
}


void IService::startWorkThread(WorkProcessor const& processor)
{
    void* sock = zmq_socket(ctx_, ZMQ_DEALER);
    zmq_connect(sock, JCONF.backend.c_str());

    zmq_pollitem_t item = {sock, 0, ZMQ_POLLIN, 0};

    int rc = 0;
    while(!stoped_) {
        rc = zmq_poll(&item, 1, -1);
        if((rc > 0) & (item.revents & ZMQ_POLLIN)) {
            zmq_msg_t identity; zmq_msg_init(&identity);
            zmq_msg_t nullframe; zmq_msg_init(&nullframe);
            zmq_msg_t client; zmq_msg_init(&client);
            zmq_msg_t content; zmq_msg_init(&content);
            int more = 1;
            size_t more_size = sizeof(more);
            while(more != 0) {
                rc = zmq_msg_recv(&identity, sock, 0);
                rc = zmq_msg_recv(&nullframe, sock, 0);
                rc = zmq_msg_recv(&client, sock, 0);
                rc = zmq_msg_recv(&content, sock, 0);
                LOG(DEBUG) << "recv : " << (char*)(zmq_msg_data(&content));
                uint32_t id = *(uint32_t*)((char*)(zmq_msg_data(&client)) + 1);
                uint32_t len = zmq_msg_size(&content);
                processor(id, (char*)zmq_msg_data(&content), len);
                zmq_getsockopt(sock, ZMQ_RCVMORE, &more, &more_size);
            }
        }
    }
}


void IService::startFrontendThread()
{
    // poll in/out
    // proxyDealers接收来自gateway的转发请求,并使用proxyJoint转发到后面的worker
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
                // 接收两帧数据: 第一张为客户端标识,第二帧为消息内容
                zmq_msg_t client_identity; zmq_msg_init(&client_identity);
                zmq_msg_t content; zmq_msg_init(&content);

                int rc = 0;
                rc = zmq_msg_recv(&client_identity, items[i].socket, 0);
                LOG(DEBUG) << "rc : " << *(uint32_t*)((char*)(zmq_msg_data(&client_identity)+1));
                rc = zmq_msg_recv(&content, items[i].socket, 0);
                LOG(DEBUG) << "rc : " << rc << " - " << (char*)(zmq_msg_data(&content));

                
                // TODO: send to router by proxyJoint;
                // 第一帧加上sock_t id
                zmq_msg_t identity; zmq_msg_init_size(&identity, sizeof(uint32_t));
                *(uint32_t*)zmq_msg_data(&identity) = ((detail::sock_t*)(&(items[i].socket)))->id;
                zmq_msg_send(&identity, proxyJoint_, ZMQ_SNDMORE);
                zmq_msg_send(&client_identity, proxyJoint_, ZMQ_SNDMORE);
                zmq_msg_send(&content, proxyJoint_, 0);

                zmq_msg_close(&client_identity);
                zmq_msg_close(&content);
                zmq_msg_close(&identity);

            }
        }
        if(items[i].revents & ZMQ_POLLIN) {
            // TODO: recv data from proxyJoint, and send by sock id;
            // 
        }
    } 
    delete []items;
}

void IService::startMonitorThread(uint32_t idx)
{

}

void IService::Stop()
{

}



} // namepsace easy




