#ifndef EASY_GATEWAY_INC
#define EASY_GATEWAY_INC

#include <thread>
#include <unordered_map>


#inlcude "zmq/zmq.h"



class ServiceCounter
{
    struct ServiceGroup {
       std::vector<uint32_t> sid_;
       uint32_t              current_;
      
       ServiceGroup()
           : current_(0)
       {
             sid_.resize(8);
       }
    }
public:
    ServiceCounter()
    {
        ctx_ = zmq_ctx_new();
        frontend_ = zmq_socket(ctx_, ZMQ_ROUTER);
        backend_ = zmq_socket(ctx_, ZMQ_ROUTER);
    }
  
    void Start()
    {
        zmq_bind(frontend_, "tcp://*:8080");
        zmq_bind(backend_, "tcp://*:8081");
        zmq_pollitem_t items[] = {
          {frontend_, 0, ZMQ_POLLIN, 0},
          {backend_, 0, ZMQ_POLLIN, 0}
        }
        
        while(true) {
            zmq_poll(items, 2, -1);
            if(items[0].revents & ZMQ_POLLIN) {
                zmq_msg_t frame0;  zmq_msg_init(&frame0);
                zmq_msg_t frame1;  zmq_msg_init(&frame1);
                zmq_msg_t frame2;  zmq_msg_init(&frame2);
                zmq_msg_t frame3;  zmq_msg_init(&frame3);
              
                zmq_msg_recv(&frame1, frontend_, 0);
                zmq_msg_recv(&frame0, frontend_, 0);
                zmq_msg_recv(&frame2, frontend_, 0);
                zmq_msg_recv(&frame3, frontend_, 0);
            }
        }
    }
  
  
private:
    void* ctx_;
    void* frontend_;
    void* backend_;
    std::unordered_map<std::string, ServiceGroup> router_;
};



#endif
