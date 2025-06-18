#ifndef _KPWN_HEXDUMP_
#define _KPWN_HEXDUMP_

#include <kpwn/logger.h>
#include <stdio.h>

extern size_t hexdump_width;

void hexdump(logf_t log, void* content, size_t len);
#endif