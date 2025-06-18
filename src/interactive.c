#include <kpwn/hexdump.h>
#include <kpwn/interactive.h>
#include <kpwn/logger.h>
#include <kpwn/memory.h>
#include <kpwn/utils.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct command_t {
  char *cmd;
  void (*handle)(int argc, char *argv[]);
};

struct command_t commands[];

char line[0x100];
char *prompt() {
  printf("kpwn> ");
  fflush(stdout);
  return fgets(line, sizeof(line), stdin);
}

int exit_interactive;

void interactive() {
  exit_interactive = 0;
  log_info("starting interactive shell");

  while (prompt()) {
    int len = strlen(line);
    if (len && line[len - 1] == '\n')
      line[len - 1] = '\0';
    char *argv[64];
    int argc = 0;
    char *tok = strtok(line, " \t");
    while (tok && argc < 64) {
      argv[argc++] = tok;
      tok = strtok(NULL, " \t");
    }
    if (argc == 0)
      continue;
    int i;
    for (i = 0; commands[i].cmd; i++) {
      if (strcmp(commands[i].cmd, argv[0]) == 0) {
        commands[i].handle(argc, argv);
        break;
      }
    }
    if (!commands[i].cmd) {
      log_erro("Unknown command: %s", argv[0]);
    }

    if (exit_interactive) {
      log_info("Exiting interactive shell");
      break;
    }
  }
}

#define UNUSED_ARG \
  (void)argc;      \
  (void)argv;

void cmd_help(int argc, char *argv[]) {
  UNUSED_ARG;
  log_info("Available commands:");
  for (int i = 0; commands[i].cmd; i++) {
    log_info("- %s", commands[i].cmd);
  }
}

void cmd_exit(int argc, char *argv[]) {
  UNUSED_ARG;
  log_info("Exiting interactive shell");
  exit(0);
}

void cmd_loglevel(int argc, char *argv[]) {
  if (argc < 2) {
    log_erro("Usage: loglevel <level>");
    return;
  }
  int level = atoi(argv[1]);
  int old = log_level;
  log_level = level;

  log_info("Log level changed from %d to %d", old,
           log_level);
}

void cmd_procinfo(int argc, char *argv[]) {
  UNUSED_ARG;
  log_info("Process information:");
  proc_info(log_info);
}

void cmd_write_memory(int argc, char *argv[]) {
  // writemem <addr> [1|2|4|8] <value>
  if (argc < 4) {
    log_erro("Usage: writemem <addr> [1|2|4|8] <value>");
    return;
  }

  void *addr = (void *)strtoull(argv[1], NULL, 0);
  size_t size = atoi(argv[2]);
  unsigned long long value = strtoull(argv[3], NULL, 0);

  if (size != 1 && size != 2 && size != 4 && size != 8) {
    log_erro(
        "Invalid size: %zu. Must be 1, 2, 4, or 8 bytes.",
        size);
    return;
  }

  switch (size) {
    case 1:
      *(uint8_t *)addr = (uint8_t)value;
      break;
    case 2:
      *(uint16_t *)addr = (uint16_t)value;
      break;
    case 4:
      *(uint32_t *)addr = (uint32_t)value;
      break;
    case 8:
      *(uint64_t *)addr = (uint64_t)value;
      break;
  }

  log_info("Wrote value %llx to %p", value, addr);
}

void cmd_read_memory(int argc, char *argv[]) {
  // readmem <addr> [1|2|4|8]
  if (argc < 3) {
    log_erro("Usage: readmem <addr> [1|2|4|8]");
    return;
  }

  void *addr = (void *)strtoull(argv[1], NULL, 0);
  size_t size = atoi(argv[2]);

  if (size != 1 && size != 2 && size != 4 && size != 8) {
    log_erro(
        "Invalid size: %zu. Must be 1, 2, 4, or 8 bytes.",
        size);
    return;
  }

  log_info("Reading %zu bytes from %p", size, addr);

  switch (size) {
    case 1:
      log_info("Value at %p: %llx", addr, *(uint8_t *)addr);
      break;
    case 2:
      log_info("Value at %p: %llx", addr,
               *(uint16_t *)addr);
      break;
    case 4:
      log_info("Value at %p: %llx", addr,
               *(uint32_t *)addr);
      break;
    case 8:
      log_info("Value at %p: %llx", addr,
               *(uint64_t *)addr);
      break;
  }
}

void cmd_vmmap(int argc, char *argv[]) {
  UNUSED_ARG;
  vmmap(log_info);
}

void cmd_hexdump(int argc, char *argv[]) {
  // hexdump <addr> <size>
  if (argc < 3) {
    log_erro("Usage: hexdump <addr> <size>");
    return;
  }

  void *addr = (void *)strtoull(argv[1], NULL, 0);
  size_t size = strtoull(argv[2], NULL, 0);

  log_info("Hexdump of %p (%zu bytes):", addr, size);
  hexdump(log_info, addr, size);
}

void cmd_continue(int argc, char *argv[]) {
  UNUSED_ARG;
  log_info("Continuing execution...");
  exit_interactive = 1;
}

struct command_t commands[] = {
    {"exit", cmd_exit},
    {"help", cmd_help},
    {"loglevel", cmd_loglevel},
    {"procinfo", cmd_procinfo},
    {"writemem", cmd_write_memory},
    {"readmem", cmd_read_memory},
    {"hexdump", cmd_hexdump},
    {"vmmap", cmd_vmmap},
    {"continue", cmd_continue},
    {NULL, NULL}};
