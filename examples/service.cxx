

#include <iostream>
#include <thread>
#include <string.h>
#include "log/easylogging++.h"

INITIALIZE_EASYLOGGINGPP

// #include "IService.hpp"
#include "zmq/zmq.h"
#include "ServiceWorker.hpp"



class DummyWorker final: public easy::ServiceWorker
{
public:
    DummyWorker(void* ctx, char const* url)
        : easy::ServiceWorker(ctx, url)
    {
    }

    virtual ~DummyWorker()
    {
    }

    virtual void Process(uint32_t id, char const* msg, uint32_t len)
    {
        char const* data = "Request has been processed.";
        Send(id, data, strlen(data));
        LOG(DEBUG) << "PROCESS DATA.";
    }
};





int main(int argc, char const* argv[])
{
    // ServiceProvider serv;
    // std::thread thr([&](){
    //     serv.Start(argv[1]);
    // });
    // thr.join();
    easy::IService<DummyWorker> serv(argv[1]);
    std::thread thr([&](){
        serv.Start();
    });
    thr.join();
    return 0;
}
