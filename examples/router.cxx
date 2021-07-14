
#include <thread>
#include <iostream>
#include "zmq/zmq.h"


int main()
{
    void* ctx = zmq_ctx_new();
    void* sock = zmq_socket(ctx, ZMQ_ROUTER);
    zmq_bind(sock, "tcp://*:9999");
    zmq_pollitem_t items = {sock, 0, ZMQ_POLLIN, 0};

    while(true) {
	/*zmq_poll(&items, 1, -1);
        if(items.revents & ZMQ_POLLIN) {
            zmq_msg_t identity; zmq_msg_init(&identity);
            zmq_msg_t service; zmq_msg_init(&service);
            
            zmq_msg_recv(&identity, sock, 0);
            zmq_msg_recv(&service, sock, 0);
            std::cout << (char*)zmq_msg_data(&service) << std::endl;	
        }*/
        zmq_msg_t identity; zmq_msg_init(&identity);
        zmq_msg_t service; zmq_msg_init(&service);
        zmq_msg_recv(&identity, sock, 0);
        zmq_msg_recv(&service, sock, 0);
        std::cout << "recv: " << (char*)zmq_msg_data(&service) << std::endl;
    }
    return 0;
}
