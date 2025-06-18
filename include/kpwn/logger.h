#ifndef _KPWN_LOGGER_
#define _KPWN_LOGGER_

#include <stdio.h>

#define LOG_INFO 0
#define LOG_WARN 1
#define LOG_ERROR 2
#define LOG_SUCCESS 3
#define LOG_QUITE 4

extern int log_level;
extern FILE* default_logfile;

void log_init();
void set_logfile(FILE* file);

typedef void logf_t(const char* fmt, ...);

#define DEFINE_LOG_FUNC(func)                      \
  void f_##func(FILE* dest, const char* fmt, ...); \
  void func(const char* fmt, ...);

DEFINE_LOG_FUNC(log_erro);
DEFINE_LOG_FUNC(log_warn);
DEFINE_LOG_FUNC(log_info);
DEFINE_LOG_FUNC(log_succ);
DEFINE_LOG_FUNC(log_null);

#undef DEFINE_LOG_FUNC

#endif