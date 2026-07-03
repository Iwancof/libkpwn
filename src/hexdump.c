#include <ctype.h>
#include <kpwn/colors.h>
#include <kpwn/hexdump.h>
#include <kpwn/logger.h>
#include <kpwn/utils.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

size_t hexdump_width = 16;

static const char *hexdump_color(uint8_t v) {
  if (isprint(v))
    return "";
  if (v == 0)
    return KPWN_COLOR_RED;
  return KPWN_COLOR_YELLOW;
}

static char hexdump_char(uint8_t v) { return isprint(v) ? v : '.'; }

void hexdump(logf_t log, void *content, size_t len) {
  uint8_t *ptr = content;

  log("-- hexdump of %p, len = %#lx --", content, len);

  for (size_t base = 0; base < len; base += hexdump_width) {
    size_t draw_len = MIN(len - base, hexdump_width);
    size_t padding_len = hexdump_width - draw_len;

    char val_buf[0x1000];
    char char_buf[0x1000];
    char *vp = val_buf;
    char *cp = char_buf;
    char *vp_end = val_buf + sizeof(val_buf);
    char *cp_end = char_buf + sizeof(char_buf);

    for (size_t i = 0; i < draw_len && vp < vp_end - 32 && cp < cp_end - 16;
         i++) {
      uint8_t v = ptr[base + i];
      const char *col = hexdump_color(v);
      char chr = hexdump_char(v);

      vp += snprintf(vp, (size_t)(vp_end - vp), "%s%02x" KPWN_COLOR_RESET " ",
                     col, v);
      cp += snprintf(cp, (size_t)(cp_end - cp), "%s%c" KPWN_COLOR_RESET, col,
                     chr);
    }

    for (size_t i = 0; i < padding_len && vp < vp_end - 4; i++)
      vp += snprintf(vp, (size_t)(vp_end - vp), "   ");

    log("> %p: %s|%s", (void *)&ptr[base], val_buf, char_buf);
  }
}
