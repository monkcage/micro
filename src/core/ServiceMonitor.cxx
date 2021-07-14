
#include "ServiceMonitor.hpp"
#include "zmq/zmq.h"


namespace easy {


ServiceMonitor::ServiceMonitor(char const* apiUrl, char const* monitorUrl)
    : apiUrl_(apiUrl)
    , monitorUrl_(monitorUrl)
{
    ctx_ = zmq_ctx_new();
    monitor_ = zmq_socket(ctx_, ZMQ_PAIR);
    api_ = zmq_socket(ctx_, ZMQ_PAIR);
}


void Start()
{
    zmq_connect(monitor_, monitorUrl_);
    zmq_connect(api_, apiUrl_);

    zmq_pollitem_t items[] = {
        { monitor_, 0, ZMQ_POLLIN, 0 },
        { api_, 0, ZMQ_POLLIN, 0}
    };
    while(true) {
        zmq_poll(items, 2, -1);
        if(items[0].revents & ZMQ_POLLIN) {
            zmq_msg_t content; zmq_msg_init(&content);
            int more = 1;
            size_t more_size = sizeof(more);
            while(more != 0) {
                zmq_msg_recv(&content, monitor_, 0);
                uint8_t const* data = (uint8_t*)zmq_msg_data(&content);
                uint32_t len = zmq_msg_size(&content);
                processMonitorMessage(data, len);

                zmq_getsockopt(monitor_, ZMQ_RCVMORE, &more, &more_size);
            }
            zmq_msg_close(&content);
        }
        if(items[1].revents & ZMQ_POLLIN) {
            zmq_msg_t content; zmq_msg_init(&content);
            int more = 1;
            size_t more_size = sizeof(more);
            while(more != 0) {
                zmq_msg_recv(&content, api_, 0);
                uint8_t const* data = (uint8_t*)zmq_msg_data(&content);
                uint32_t len = zmq_msg_size(&content);
                processApiMessage(data, len);

                zmq_getsockopt(api_, ZMQ_RCVMORE, &more, &more_size);
            }
        }
    }
}


void ServiceMonitor::processMonitorMessage(uint8_t const* data, uint32_t len)
{
    uint16_t event = *(uint16_t*)(data);      // 事件类型
    uint32_t value = *(uint32_t*)(data + 2);  // event value(一般为raw fd)
    if(ZMQ_EVENT_DISCONNECTED == event) {     // 断开连接,从Router中删除对应的服务

    }else if (ZMQ_EVENT_ACCEPTED) {           // 建立连接

    }
}


void ServiceMonitor::processApiMessage(uint8_t const* data, uint32_t len)
{

}


} // namespace easy