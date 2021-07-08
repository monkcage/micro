
#include <iostream>
#include <thread>
#include <string.h>


#include "zmq/zmq.h"



class ServiceClaim
{
public:
    ServiceClaim()
    {
        ctx_ = zmq_ctx_new();
        sock_ = zmq_socket(ctx_, ZMQ_REQ);
    }

    void Start(char const* srvName)
    {
        zmq_connect(sock_, "tcp://127.0.0.1:8080");
        int idx = 1;
        while(true) {
            zmq_msg_t service;
            zmq_msg_t message;
            char const* data = "I am client x";
            char buff[215] {0};
            sprintf(buff, "%s%d", srvName, idx++);

            zmq_msg_init_size(&service, strlen(buff) + 1);
            zmq_msg_init_size(&message, strlen(data) + 1);
            memcpy(zmq_msg_data(&service), buff, strlen(buff));
            memcpy(zmq_msg_data(&message), data, strlen(data));
            zmq_msg_send(&service, sock_, ZMQ_SNDMORE);
            zmq_msg_send(&message, sock_, 0);

            zmq_msg_t resp;
            zmq_msg_init(&resp);
            zmq_msg_recv(&resp, sock_, 0);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

private:
    void* ctx_;
    void* sock_;
};



int main(int argc, char const* argv[])
{
    ServiceClaim serv;
    std::thread thr([&](){
        serv.Start(argv[1]);
    });
    thr.join();
    return 0;
}
