
#include <thread>
#include "ServiceCounter.hpp"
#include "log/easylogging++.h"

INITIALIZE_EASYLOGGINGPP


int main(int argc, char const* argv[])
{
    easy::ServiceCounter serv(argv[1]);
    std::thread thr([&](){
        serv.Start();
    });
    thr.join();
    return 0;
}
