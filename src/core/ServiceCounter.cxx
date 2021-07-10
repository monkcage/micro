#include "log/easylogging++.h"
#include "ServiceRouter.hpp"
#include "ServiceCounter.hpp"



namespace easy {



ServiceCounter::ServiceCounter()
    : router_(std::make_shared<ServiceRouter>())
{
    ctx_ = zmq_ctx_new();
    frontend_ = zmq_socket(ctx_, ZMQ_ROUTER);
    backend_ = zmq_socket(ctx_, ZMQ_ROUTER);
    int linger = 1000;
    zmq_setsockopt(frontend_, ZMQ_LINGER, &linger, sizeof(linger));
    zmq_setsockopt(backend_, ZMQ_LINGER, &linger, sizeof(linger));
}


void ServiceCounter::Start()
{
    zmq_bind(frontend_, "tcp://*:8080");
    zmq_bind(backend_, "tcp://*:8081");
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
            }else{
                uint32_t id = *(uint32_t*)((char*)zmq_msg_data(&identity) + 1);
                LOG(INFO) << "register service: conn(" << id << ") - " << (char*)(zmq_msg_data(&service)); 
                router_->RegisterService(id, (char*)(zmq_msg_data(&service)));
                zmq_msg_t dummy; zmq_msg_init(&dummy);
                zmq_msg_send(&identity, backend_, ZMQ_SNDMORE);
                zmq_msg_send(&dummy, backend_, 0);
                zmq_msg_close(&dummy);
            }
        }
    }
}


}