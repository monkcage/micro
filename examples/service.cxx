

#include <iostream>
#include <thread>
#include <string.h>


#include "zmq/zmq.h"



class ServiceProvider
{
public:
    ServiceProvider()
    {
        ctx_ = zmq_ctx_new();
        sock_ = zmq_socket(ctx_, ZMQ_DEALER);
    }

    void Start(char const* srvName)
    {
        zmq_connect(sock_, "tcp://127.0.0.1:8081");

        zmq_msg_t nullframe;
        zmq_msg_t identity;
        zmq_msg_t service;

        zmq_msg_init(&nullframe);
        zmq_msg_init_size(&identity, strlen(srvName) + 1);
        zmq_msg_init_size(&service, strlen(srvName) + 1);

        memcpy(zmq_msg_data(&service), srvName, strlen(srvName) + 1);
        zmq_msg_send(&service, sock_, 0);

        zmq_pollitem_t items {sock_, 0, ZMQ_POLLIN, 0};
        while(true) {
            zmq_poll(&items, 1, -1);
            if(items.revents & ZMQ_POLLIN) {
                zmq_msg_t client;  zmq_msg_init(&client);
                zmq_msg_t content; zmq_msg_init(&content);

                zmq_msg_recv(&client, sock_, 0);
                zmq_msg_recv(&content, sock_, 0);

                char* message = "I got your message";
                zmq_msg_t resp; zmq_msg_init_size(&resp, strlen(message) + 1);
                memcpy(zmq_msg_data(&resp), message, strlen(message) + 1);
                zmq_msg_send(&client, sock_, ZMQ_SNDMORE);
                zmq_msg_send(&resp, sock_, 0);
            }
        }
    }

private:
    void* ctx_;
    void* sock_;
};



int main(int argc, char const* argv[])
{
    ServiceProvider serv;
    std::thread thr([&](){
        serv.Start(argv[1]);
    });
    thr.join();
    return 0;
}
