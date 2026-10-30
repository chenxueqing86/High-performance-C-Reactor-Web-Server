#include "AsyncLogging.h"
#include "Logging.h"
#include <iostream>
#include <thread>
#include <vector>
#include <unistd.h>

//AsyncLogging* g_asynclog = nullptr;

void worker(int id){
    for(int i = 0;i < 10000;++i){
        LOG_INFO("Thread %d is writing log number %d",id,i);
    }
}

int main(){
//    AsyncLogging log("test.log",3);  // 3 秒超时刷盘
 //   g_asynclog = &log;
 AsyncLogging* log = AsyncLogging::getInstance("test.log",3);
   // log.start();
   log->start();

    std::cout << "Start testing AsyncLogging..." << std::endl;

    std::vector<std::thread> threads;
    for(int i = 0;i < 4;++i){
        threads.emplace_back(worker,i);
    }

     for(auto& t: threads){
        t.join();
     }

     std::cout << "All threads done,waiting for flush..." << std::endl;
     sleep(1); //等后台把最后的日志写完

    // log.stop();
    log->stop();
     return 0;
     
}