
#include "ServiceMonitor.hpp"
#include "zmq/zmq.h"


namespace easy {


ServiceMonitor::ServiceMonitor(char const* apiUrl, char const* monitorUrl)
    : apiUrl_(apiUrl)
    , monitorUrl_(monitorUrl)
{
    ctx_ = zmq_ctx_new();
    monitor_ = zmq_socket(ctx_, ZMQ_PAIR);
    api_ = zmq_socket(ctx_, ZMQ_PUSH);
}


void Start()
{

}



} // namespace easy