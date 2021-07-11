#ifndef EASY_CORE_SERVICE_INC
#define EASY_CORE_SERVICE_INC

#include <vector>
#include <thread>
#include <iostream>
#include <fstream>

#include "zmq/zmq.h"
#include "log/easylogging++.h"
#include "nlohmann/json.hpp"

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


template <typename WK>
class IService {
public:
    IService(char const* confile)
        : index_(0)
        , stoped_(false)
    {
        json json_conf;
        std::ifstream infile(confile);
        infile >> json_conf;
        JCONF = json_conf.get<detail::service_conf_t>();

        ctx_ = zmq_ctx_new();
    } 

    virtual ~IService()
    {
        for(auto&& thr: threads_) {
            if(thr.joinable()) 
                thr.join();
        }
    }

    void Start()
    {
        // 先开启后端处理线程:包括proxy线程和worker线程
        proxyRouter_ = zmq_socket(ctx_, ZMQ_ROUTER);
        proxyDealer_ = zmq_socket(ctx_, ZMQ_DEALER);
        zmq_bind(proxyRouter_, URI_PROXY_ROUTER);
        zmq_bind(proxyDealer_, JCONF.backend.c_str());
        threads_.emplace_back(std::thread(std::bind(&IService::startBackendProxyThread, this, proxyRouter_, proxyDealer_)));
        // 开启一个worker用于测试
        threads_.emplace_back(std::thread(std::bind(&IService::startWorkThread, this)));
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

    void Stop()
    {
        stoped_ = true;
    }

    void* Context()
    {
        return ctx_;
    }

private:
    // void startBackendThread();
    // void startFrontendThread(); 
    // void startMonitorThread(uint32_t idx);

    void startFrontendProxyThread(void* frontend, void* backend)
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
                // zmq_msg_t identity; zmq_msg_init(&identity);
                // zmq_msg_t nullframe; zmq_msg_init(&nullframe);
                zmq_msg_t client; zmq_msg_init(&client);
                zmq_msg_t content; zmq_msg_init(&content);
                int more = 1;
                size_t more_size = sizeof(more);
                int rc = 0;
                while(more != 0) {
                    // rc = zmq_msg_recv(&identity, backend, 0);
                    // zmq_msg_recv(&nullframe, backend, ZMQ_RCVMORE);
                    // LOG(DEBUG) << "rc : " << rc; 
                    rc = zmq_msg_recv(&client, backend, 0);
                    LOG(DEBUG) << "rc : " << rc;
                    // LOG(DEBUG) << "RECV: " << (char*)zmq_msg_data(&identity);
                    rc = zmq_msg_recv(&content, backend, 0);
                    LOG(DEBUG) << "RECV: " << rc << " - " << (char*)zmq_msg_data(&content);
                    zmq_msg_send(&client, frontend, ZMQ_SNDMORE);
                    zmq_msg_send(&content, frontend, 0);
                    zmq_getsockopt(&backend, ZMQ_RCVMORE, &more, &more_size);
                }
                // zmq_msg_close(&identity);
                // zmq_msg_close(&nullframe);
                zmq_msg_close(&client);
                zmq_msg_close(&content);
            }
        }
    }

    void startBackendProxyThread(void* frontend, void* backend)
    {
        zmq_proxy(frontend, backend, nullptr);
    }

    void startWorkThread()
    {
        auto worker = std::make_shared<WK>(ctx_, JCONF.backend.c_str());
        workers_.emplace_back(worker);
        worker->Start();
    }

    

private:
    void* ctx_;
    std::vector<void*> proxyDealers_;
    uint32_t           index_;
    void* proxyJoint_;
    void* proxyRouter_;
    void* proxyDealer_;
    bool  stoped_;
    std::vector<std::thread> threads_;
    std::vector<std::shared_ptr<WK>> workers_;
};



} // namespace easy



#endif 