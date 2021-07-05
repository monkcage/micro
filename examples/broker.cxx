
#include <thread>
#include "ServiceCounter.hpp"


int main()
{
    easy::ServiceCounter serv;
    std::thread thr([&](){
        serv.Start();
    });
    thr.join();
    return 0;
}
