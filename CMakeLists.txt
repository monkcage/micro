

CMAKE_MINIMUM_REQUIRED(VERSION 3.8.0)

PROJECT(micro)

ADD_DEFINITIONS(-std=c++11)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")

INCLUDE_DIRECTORIES(./th3party/inc ./src/core ./src)

LINK_DIRECTORIES(./th3party/lib)



ADD_EXECUTABLE(client ./examples/client.cxx)
TARGET_LINK_LIBRARIES(client zmq)


ADD_EXECUTABLE(broker ./examples/broker.cxx 
                      ./src/core/ServiceCounter.cxx 
                      ./src/core/ServiceRouter.cxx
                      ./src/log/easylogging++.cc
                      ./src/pugixml.cpp)
TARGET_LINK_LIBRARIES(broker zmq)


ADD_EXECUTABLE(server ./examples/service.cxx
                      ./src/log/easylogging++.cc)
TARGET_LINK_LIBRARIES(server zmq)

ADD_EXECUTABLE(router ./examples/router.cxx)
TARGET_LINK_LIBRARIES(router zmq)
#ADD_EXECUTABLE(test ./examples/test.cxx 
#                    ./src/IService.cxx
#                    ./src/log/easylogging++.cc)
#TARGET_LINK_LIBRARIES(test zmq)

ADD_EXECUTABLE(REQ ./examples/req.cxx)
TARGET_LINK_LIBRARIES(REQ zmq)
