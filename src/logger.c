#include <kpwn/logger.h>
#include <stdarg.h>
#include <stdio.h>

int log_level = LOG_ERROR;
FILE* default_logfile;

#define LOG_COLOR_RESET "\033[0m"
#define LOG_COLOR_RED "\033[31m"
#define LOG_COLOR_YELLOW "\033[33m"
#define LOG_COLOR_BLUE "\033[34m"
#define LOG_COLOR_GREEN "\033[32m"

__attribute__((constructor)) void log_init() {
  default_logfile = stdout;
}

void set_logfile(FILE* file) {
  default_logfile = file;
}

static const char* log_prefix(int level) {
  switch (level) {
    case LOG_DEBUG:
      return "[ " LOG_COLOR_RED "DEBG" LOG_COLOR_RESET " ]";
    case LOG_INFO:
      return "[ " LOG_COLOR_BLUE "INFO" LOG_COLOR_RESET
             " ]";
    case LOG_WARN:
      return "[ " LOG_COLOR_YELLOW "WARN" LOG_COLOR_RESET
             " ]";
    case LOG_ERROR:
      return "[ " LOG_COLOR_RED "ERRO" LOG_COLOR_RESET " ]";
    case LOG_SUCCESS:
      return "[ " LOG_COLOR_GREEN "SUCC" LOG_COLOR_RESET
             " ]";
    default:
      return "[ UNKN ]";
  }
}

void log_impl(FILE* dest, int level, const char* fmt,
              va_list list) {
  if (level < log_level)
    return;

  fprintf(dest, "%s", log_prefix(level));
  fputc(' ', dest);
  vfprintf(dest, fmt, list);
  fputc('\n', dest);
  fflush(dest);
}

#define DEFINE_LOG_FUNC(func, level)                \
  void f_##func(FILE* dest, const char* fmt, ...) { \
    va_list list;                                   \
    va_start(list, fmt);                            \
    log_impl(dest, level, fmt, list);               \
    va_end(list);                                   \
  }                                                 \
  void func(const char* fmt, ...) {                 \
    va_list list;                                   \
    va_start(list, fmt);                            \
    log_impl(default_logfile, level, fmt, list);    \
    va_end(list);                                   \
  }

DEFINE_LOG_FUNC(log_debug, LOG_DEBUG);
DEFINE_LOG_FUNC(log_info, LOG_INFO);
DEFINE_LOG_FUNC(log_warn, LOG_WARN);
DEFINE_LOG_FUNC(log_error, LOG_ERROR);
DEFINE_LOG_FUNC(log_success, LOG_SUCCESS);

void f_log_null(FILE* dest, const char* fmt, ...) {
  (void)dest;
  (void)fmt;
}

void log_null(const char* fmt, ...) {
  (void)fmt;
}
