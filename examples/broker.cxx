
#include <thread>
#include "ServiceCounter.hpp"
#include "log/easylogging++.h"

INITIALIZE_EASYLOGGINGPP


int main()
{
    easy::ServiceCounter serv;
    std::thread thr([&](){
        serv.Start();
    });
    thr.join();
    return 0;
}
