#ifndef EASY_CORE_SERVICE_WORKER_INC
#define EASY_CORE_SERVICE_WORKER_INC

#include "IService.hpp"
#include "zmq/zmq.h"
#include "log/easylogging++.h"


namespace easy {

class ServiceWorker 
{
public:
    ServiceWorker(void* ctx, char const* url)
        : ctx_(ctx)
        , url_(url)
    {
    }

    virtual ~ServiceWorker() = default;

    void Start()
    {
        sock_ = zmq_socket(ctx_, ZMQ_DEALER);
        zmq_connect(sock_, url_);

        zmq_pollitem_t item = {sock_, 0, ZMQ_POLLIN, 0};

        int rc = 0;
        while(true) {
            rc = zmq_poll(&item, 1, -1);
            if((rc > 0) & (item.revents & ZMQ_POLLIN)) {
                zmq_msg_t identity; zmq_msg_init(&identity);
                zmq_msg_t nullframe; zmq_msg_init(&nullframe);
                zmq_msg_t client; zmq_msg_init(&client);
                zmq_msg_t content; zmq_msg_init(&content);
                int more = 1;
                size_t more_size = sizeof(more);
                while(more != 0) {
                    rc = zmq_msg_recv(&identity, sock_, 0);
                    rc = zmq_msg_recv(&nullframe, sock_, 0);
                    rc = zmq_msg_recv(&client, sock_, 0);
                    rc = zmq_msg_recv(&content, sock_, 0);
                    identity_ = *(uint32_t*)((char*)(zmq_msg_data(&identity)) + 1);
                    LOG(DEBUG) << "recv : " << ((char*)(zmq_msg_data(&content)));
                    uint32_t id = *(uint32_t*)((char*)(zmq_msg_data(&client)) + 1);
                    client_ = id;
                    uint32_t len = zmq_msg_size(&content);
                    Process(id, (char*)zmq_msg_data(&content), len);
                    zmq_getsockopt(sock_, ZMQ_RCVMORE, &more, &more_size);
                }
            }
        }
    }


    virtual void Process(uint32_t id, char const* msg, uint32_t len) = 0;

    void Send(uint32_t id, char const* msg, uint32_t len)
    {
        zmq_msg_t identity; zmq_msg_init_size(&identity, 5);
        zmq_msg_t client; zmq_msg_init_size(&client, 5);
        zmq_msg_t content; zmq_msg_init_size(&content, len);
        char* data = (char*)zmq_msg_data(&identity);
        *data = 0;
        *(uint32_t*)((char*)data + 1) = identity_;
        memcpy(zmq_msg_data(&content), msg, len);

        char* cdata = (char*)zmq_msg_data(&client);
        *cdata = 0;
        *(uint32_t*)((char*)cdata + 1) = client_;
        memcpy(zmq_msg_data(&content), msg, len);

        zmq_msg_send(&identity, sock_, ZMQ_SNDMORE);
        int rc = zmq_msg_send(&client, sock_, ZMQ_SNDMORE);
        // LOG(DEBUG) << "RC : " << rc;
        rc = zmq_msg_send(&content, sock_, 0);
        LOG(DEBUG) << "RC : " << rc;
    }

private:
    void* ctx_;
    void* sock_;
    char const* url_;

    uint32_t identity_;
    uint32_t client_;
};


} // namespace easy


#endif