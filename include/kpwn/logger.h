#ifndef _KPWN_LOGGER_
#define _KPWN_LOGGER_

#include <stdio.h>

#define LOG_DEBUG 0
#define LOG_INFO 1
#define LOG_WARN 2
#define LOG_ERROR 3
#define LOG_SUCCESS 4
#define LOG_QUIET 5
#define LOG_QUITE LOG_QUIET /* compat alias for the old typo */

extern int log_level;
extern FILE *default_logfile;

void log_init();
void set_logfile(FILE *file);

typedef void logf_t(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#define DEFINE_LOG_FUNC(func)                                                  \
  void f_##func(FILE *dest, const char *fmt, ...)                              \
      __attribute__((format(printf, 2, 3)));                                   \
  void func(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

DEFINE_LOG_FUNC(log_debug);
DEFINE_LOG_FUNC(log_info);
DEFINE_LOG_FUNC(log_warn);
DEFINE_LOG_FUNC(log_error);
DEFINE_LOG_FUNC(log_success);
DEFINE_LOG_FUNC(log_null);

#undef DEFINE_LOG_FUNC

// LLM-friendly output macros — machine-parseable with grep '[kpwn:leak]' etc.

#define KPWN_LEAK(name, val)                                                   \
  log_success("[kpwn:leak] %s=0x%lx", (name), (unsigned long)(val))

#define KPWN_DUMP(expr)                                                        \
  log_info("[kpwn:dump] " __FILE__ ":%d %s = 0x%lx (%ld)", __LINE__, #expr,    \
           (unsigned long)(expr), (long)(expr))

#endif
