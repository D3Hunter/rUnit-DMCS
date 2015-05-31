////////////////////////////////////////////////////////////////////////////////
//               Copyright(c) 2013 SDU, All right reserved
// Filename        :    common.h
// Projectname     :    SDU Data Management
// Author          :    Jujj
// Email           :    
// Date            :    周四 十月 31 15:21:23 2013 (+0800)
// Version         :    v 1.0
// Description     :    
////////////////////////////////////////////////////////////////////////////////
#ifndef __COMMON_H__
#define __COMMON_H__
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifndef DEBUG
    #define DEBUG
#endif

#ifdef DEBUG
#define DEBUG_PRINT_HEADER ">>>>>> In DM %2d <<<<<< : "
#define DEBUG_PRINT(format, ...) printf(DEBUG_PRINT_HEADER format, dm_cfg.rank, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(format, ...)
#endif

int isFDValid(int fd);

// #define OVERALL_EPOCH_FLUSH
// #define MUTEX_EXCLUSIVE_READ
// #define LOCAL_READ
#define NATIVE_RDMA

#endif
