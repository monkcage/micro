#ifndef EASY_SERVICE_CLAIM_INC
#define EASY_SERVICE_CLAIM_INC

#include <string.h>
#include <string>

#include "nanomsg/nn.h"
#include "nanomsg/survey.h"


class ServiceClaim
{
public:
    ServiceClaim()
    {
        claim_ = nn_socket(AF_SP, NN_SURVEYOR);
        // Generally, we can connect to multi broker for node failure
        nn_connect(claim_, "tcp://127.0.0.1:8080");
    }
  
    // just confirm there has SERVICE(S) named service
    bool Exists(std::string const& service);
    bool Exists(std::string&& service);
    bool Exists(char const* service);
  
    // just confirm there has SERVICE(S) named serice in cahce
    bool CacheExists(std::string const& service);
    bool CacheExists(std::string&& service);
    bool CacheExists(char const* service);
private:
    int claim_;
};


#endif
