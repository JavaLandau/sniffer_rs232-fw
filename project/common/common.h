#ifndef __COMMON_H__
#define __COMMON_H__

#define RES_OK                  0
#define RES_NOK                 1
#define RES_INVALID_PAR         2
#define RES_MEMORY_ERR          3
#define RES_TIMEOUT             4
#define RES_NOT_SUPPORTED       5
#define RES_OVERFLOW            6
#define RES_NOT_INITIALIZED     7
#define RES_NOT_ALLOWED         8

#define ARRAY_SIZE(X)       (sizeof(X) / sizeof(X[0]))
#define MIN(X, Y)           (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)           (((X) > (Y)) ? (X) : (Y))

#endif