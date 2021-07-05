#ifndef EASY_SERVICE_COUNTER_INC
#define EASY_SERVICE_COUNTER_INC

#include <memory>

namespace easy {

class ServiceRouter;

class ServiceCounter 
{
public:
    ServiceCounter();

    void Start();

private:
    void* ctx_;
    void* frontend_;
    void* backend_;
    std::shared_ptr<ServiceRouter> router_;
};


}

#endif