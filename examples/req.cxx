#include <iostream>
#include <thread>
#include <string.h>

#include "zmq/zmq.h"


int main()
{
    void* ctx = zmq_ctx_new();
    void* sock = zmq_socket(ctx, ZMQ_REQ);
    zmq_connect(sock, "tcp://192.168.1.106:8081");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    char const* msg = "hello";
    zmq_msg_t content; zmq_msg_init_size(&content, strlen(msg));
    memcpy(zmq_msg_data(&content), msg, strlen(msg));
    zmq_msg_send(&content, sock, 0);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    zmq_msg_t resp; zmq_msg_init(&resp);
    zmq_msg_recv(&resp, sock, 0);
    std::cout << "resp : " << (char*)zmq_msg_data(&resp) << std::endl;
    return 0;
}
