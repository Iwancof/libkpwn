#include <kpwn/colors.h>
#include <kpwn/logger.h>
#include <stdarg.h>
#include <stdio.h>

int log_level = LOG_INFO;
FILE *default_logfile;

__attribute__((constructor)) void log_init() { default_logfile = stdout; }

void set_logfile(FILE *file) { default_logfile = file; }

static const char *log_prefix(int level) {
  switch (level) {
  case LOG_DEBUG:
    return "[ " KPWN_COLOR_DIM "DEBG" KPWN_COLOR_RESET " ]";
  case LOG_INFO:
    return "[ " KPWN_COLOR_BLUE "INFO" KPWN_COLOR_RESET " ]";
  case LOG_WARN:
    return "[ " KPWN_COLOR_YELLOW "WARN" KPWN_COLOR_RESET " ]";
  case LOG_ERROR:
    return "[ " KPWN_COLOR_RED "ERRO" KPWN_COLOR_RESET " ]";
  case LOG_SUCCESS:
    return "[ " KPWN_COLOR_GREEN "SUCC" KPWN_COLOR_RESET " ]";
  default:
    return "[ UNKN ]";
  }
}

void log_impl(FILE *dest, int level, const char *fmt, va_list list) {
  if (level < log_level)
    return;

  fprintf(dest, "%s", log_prefix(level));
  fputc(' ', dest);
  vfprintf(dest, fmt, list);
  fputc('\n', dest);
  fflush(dest);
}

#define DEFINE_LOG_FUNC(func, level)                                           \
  void f_##func(FILE *dest, const char *fmt, ...) {                            \
    va_list list;                                                              \
    va_start(list, fmt);                                                       \
    log_impl(dest, level, fmt, list);                                          \
    va_end(list);                                                              \
  }                                                                            \
  void func(const char *fmt, ...) {                                            \
    va_list list;                                                              \
    va_start(list, fmt);                                                       \
    log_impl(default_logfile, level, fmt, list);                               \
    va_end(list);                                                              \
  }

DEFINE_LOG_FUNC(log_debug, LOG_DEBUG);
DEFINE_LOG_FUNC(log_info, LOG_INFO);
DEFINE_LOG_FUNC(log_warn, LOG_WARN);
DEFINE_LOG_FUNC(log_error, LOG_ERROR);
DEFINE_LOG_FUNC(log_success, LOG_SUCCESS);

void f_log_null(FILE *dest, const char *fmt, ...) {
  (void)dest;
  (void)fmt;
}

void log_null(const char *fmt, ...) { (void)fmt; }
