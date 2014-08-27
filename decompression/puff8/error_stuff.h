#ifndef _ERROR_STUFF_H_INCLUDED
#define _ERROR_STUFF_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>

#define CHECK_ERROR(condition,message) \
do {if (condition) { \
    fprintf(stderr, "%s:%d:%s: %s\n",__FILE__,__LINE__,__func__,message); \
    abort(); \
}}while(0)

#define CHECK_ERRNO(condition, message) \
do {if (condition) { \
    fprintf(stderr, "%s:%d:%s:%s: ",__FILE__,__LINE__,__func__,message); \
    fflush(stderr); \
    perror(NULL); \
    abort(); \
}}while(0)

#define CHECK_FILE(condition,file,message) \
do {if (condition) { \
    fprintf(stderr, "%s:%d:%s:%s: ",__FILE__,__LINE__,__func__,message); \
    fflush(stderr); \
    if (feof(file)) { \
        fprintf(stderr,"unexpected EOF\n"); \
    } else { \
        perror(message); \
    } \
    abort(); \
}}while(0)

#endif /* _ERROR_STUFF_H_INCLUDED */
