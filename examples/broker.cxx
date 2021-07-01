
#include "ServiceCounter.hpp"


int main()
{
    ServiceCounter serv;
    std::thread thr([&](){
        serv.Start();
    });
    thr.join();
    return 0;
}
