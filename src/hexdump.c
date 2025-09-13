#include <ctype.h>
#include <kpwn/hexdump.h>
#include <kpwn/logger.h>
#include <kpwn/utils.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

size_t hexdump_width = 16;

#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"

static const char *hexdump_color(uint8_t v) {
  int alnum = isprint(v);
  int is_null = v == 0;

  if (alnum)
    return "";
  else if (is_null)
    return COLOR_RED;
  else
    return COLOR_YELLOW;
}

static char hexdump_char(uint8_t v) {
  int alnum = isprint(v);

  if (alnum)
    return v;
  else
    return '.';
}

#define MAX_BUFFER_SIZE 0x1000

void hexdump(logf_t log, void *content, size_t len) {
  uint8_t *ptr = content;
  static char val_buf[MAX_BUFFER_SIZE], char_buf[MAX_BUFFER_SIZE];

  log("-- hexdump of %p, len = %#lx --", content, len);

  for (size_t base = 0; base < len; base += hexdump_width) {
    size_t draw_len = MIN(len - base, hexdump_width);
    size_t padding_len = hexdump_width - draw_len;

    char *cur_vp = val_buf;
    char *cur_cp = char_buf;

#define append(buf, ptr, ...)                                                  \
  ptr += snprintf(ptr, buf + MAX_BUFFER_SIZE - ptr, __VA_ARGS__);

    append(val_buf, cur_vp, "%*c", (int)(1 + 3 * padding_len), ' ');
    append(char_buf, cur_cp, "%*c", (int)(1 + padding_len), ' ');

    for (ssize_t i = draw_len - 1; 0 <= i; i--) {
      uint8_t v = ptr[base + i];
      const char *col = hexdump_color(v);
      char chr = hexdump_char(v);

      append(val_buf, cur_vp, "%s%02x" COLOR_RESET " ", col, v);
      append(char_buf, cur_cp, "%s%c" COLOR_RESET, col, chr);
    }

    if (cur_vp == &val_buf[MAX_BUFFER_SIZE]) {
      log_warn("hexdump: hexdump_width is too large, "
               "truncating output");
    }

    log("> %p:%s|%s", &ptr[base], val_buf, char_buf);
  }
}
