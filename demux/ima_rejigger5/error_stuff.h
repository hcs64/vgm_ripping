#ifndef _ERROR_STUFF_H_INCLUDED
#define _ERROR_STUFF_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG
#   define DEBUG_EXIT abort()
#else
#   define DEBUG_EXIT exit(EXIT_FAILURE)
#endif

#define CHECK_ERROR(condition,message) \
do {if (condition) { \
    fflush(stdout); \
    fprintf(stderr, "%s:%d:%s: %s\n",__FILE__,__LINE__,__func__,message); \
    DEBUG_EXIT; \
}}while(0)

#define CHECK_ERROR_RETURN(condition,message) \
do {if (condition) { \
    fflush(stdout); \
    fprintf(stderr, "%s:%d:%s: %s\n",__FILE__,__LINE__,__func__,message); \
    return -1; \
}}while(0)

#define CHECK_ERRNO(condition, message) \
do {if (condition) { \
    fflush(stdout); \
    fprintf(stderr, "%s:%d:%s:%s: ",__FILE__,__LINE__,__func__,message); \
    fflush(stderr); \
    perror(NULL); \
    DEBUG_EXIT; \
}}while(0)

#define CHECK_FILE(condition,file,message) \
do {if (condition) { \
    fflush(stdout); \
    fprintf(stderr, "%s:%d:%s:%s: ",__FILE__,__LINE__,__func__,message); \
    fflush(stderr); \
    if (feof(file)) { \
        fprintf(stderr,"unexpected EOF\n"); \
    } else { \
        perror(message); \
    } \
    DEBUG_EXIT; \
}}while(0)

#endif /* _ERROR_STUFF_H_INCLUDED */
