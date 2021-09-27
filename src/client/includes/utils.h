// @author Luca Cirillo (545480)

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdlib.h>

#define REQUEST_LENGTH 2048

int readn(long fd, void* buf, size_t size);
int writen(long fd, void* buf, size_t size);
int is_number(const char* arg, long* num);

#endif
