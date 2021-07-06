
#include <iostream>
#include "IService.hpp"


struct test_t{
    void* a;
    int   b;
};


int main(int argc, char const* argv[])
{
    easy::IService serv(argv[1]);
    serv.Start();
    test_t* ts = new test_t;
    ts->a = new int;
    ts->b = 2;

    test_t* t = reinterpret_cast<test_t*>(&ts->a);
    std::cout << "ts point a: " << ts->a << std::endl;
    //std::cout << "ts point in: " << (void*)*ts << std::endl;
    return 0;
}


