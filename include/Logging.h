#pragma once

#include <string>
#include <cstring>
#include <cstdio>
#include "AsyncLogging.h"
#include <sys/time.h>

/*
//全局单例指针
extern AsyncLogging* g_asyncLog;

// 基础写入函数

#define LOG_BASE(level,format,...)\ 
       do { \
          char buf[1024]; \
          struct timeval tv; \
          gettimeofday(&tv,NULL); \
          struct tm tm_time; \
          localtime_r(&tv.tv_sec,&tm_time); \
          int len = snprintf(buf,sizeof(buf),\
                "[%s] [%04d-%02d-%02d %02d:%02d:%02d.%06ld]" format "\n",\
               level,\
                tm_time.tm_year + 1990,tm_time.tm_mon + 1,tm_time.tm_mday,\
               tm_time.tm_hour,tm_time.tm_min,tm_time.tm_sec,tv.tv_usec, \
               ##__VA_ARGS__); \
               if(g_asyncLog) g_asyncLog->append(buf,len);\
          }while(0)
          
          #define LOG_INFO(format,...) LOG_BASE("INFO",format,##__VA_ARGS__)
#define LOG_WARN(format,...) LOG_BASE("WARN",format,##__VA_ARGS__)
#define LOG_ERROR(format,...) LOG_BASE("ERROR",format,##__VA_ARGS__)
*/
          // 注意：所有的 '\' 后面绝对不能有任何空格！

          // 声明全局指针
extern AsyncLogging* g_asyncLog;


#define LOG_BASE(level, format, ...) \
    do { \
        char buf[1024]; \
        struct timeval tv; \
        gettimeofday(&tv, NULL); \
        struct tm tm_time; \
        localtime_r(&tv.tv_sec, &tm_time); \
        int len = snprintf(buf, sizeof(buf), \
            "[%s] [%04d-%02d-%02d %02d:%02d:%02d.%06ld] " format "\n", \
            level, \
            tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday, \
            tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, tv.tv_usec, \
            ##__VA_ARGS__); \
        AsyncLogging::getInstance() ->append(buf,len); \
    } while (0)
    //  if (g_asyncLog) g_asyncLog->append(buf, len); 


    #define LOG_INFO(format, ...)  LOG_BASE("INFO", format, ##__VA_ARGS__)
    #define LOG_WARN(format, ...)  LOG_BASE("WARN", format, ##__VA_ARGS__)
    #define LOG_ERROR(format, ...) LOG_BASE("ERROR", format, ##__VA_ARGS__)


