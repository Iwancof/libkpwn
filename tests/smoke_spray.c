// smoke_spray.c — real-kernel smoke tests for spray primitives.
// Run as: gcc -O0 -ggdb3 -Wall -Wextra -I./include smoke_spray.c src/*.c
//         src/x86_64/*.c src/x86_64/*.s -o smoke_spray && ./smoke_spray
// Some tests need CAP_NET_RAW (pgv) or root. Failures are logged, not fatal.

#define _GNU_SOURCE
#include <kpwn/kpwn.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int test_msg_spray(void) {
  int qids[8];
  char data[200];
  memset(data, 'A', sizeof(data));

  int rc = kpwn_spray_msg(qids, 8, data, sizeof(data));
  if (rc != 0) {
    log_error("[smoke] msg_spray alloc failed: %d", rc);
    return 1;
  }

  // read back from first queue
  char buf[200] = {0};
  int n = kpwn_read_msg(qids[0], buf, sizeof(buf));
  if (n < 0) {
    log_error("[smoke] msg_read failed: %d", n);
    kpwn_free_msg(qids, 8);
    return 1;
  }

  if (memcmp(buf, data, sizeof(data)) != 0) {
    log_error("[smoke] msg_read data mismatch!");
    kpwn_free_msg(&qids[1], 7);
    return 1;
  }

  kpwn_free_msg(&qids[1], 7);
  log_success("[smoke] msg_msg spray+read+free: OK");
  return 0;
}

static int test_pipe_spray(void) {
  struct kpwn_pipe pipes[4];
  int rc = kpwn_spray_pipe(pipes, 4);
  if (rc != 0) {
    log_error("[smoke] pipe_spray failed: %d", rc);
    return 1;
  }

  char data[64];
  memset(data, 'B', sizeof(data));
  rc = kpwn_write_pipe(pipes, 4, data, sizeof(data));
  if (rc != 0) {
    log_error("[smoke] pipe_write failed: %d", rc);
    kpwn_free_pipe(pipes, 4);
    return 1;
  }

  // Read back from first pipe
  char buf[64] = {0};
  ssize_t n = read(pipes[0].fd[0], buf, sizeof(buf));
  if (n != sizeof(data) || memcmp(buf, data, sizeof(data)) != 0) {
    log_error("[smoke] pipe_read data mismatch! n=%zd", n);
    kpwn_free_pipe(pipes, 4);
    return 1;
  }

  kpwn_free_pipe(pipes, 4);
  log_success("[smoke] pipe spray+write+read+free: OK");
  return 0;
}

static int test_skb_spray(void) {
  struct kpwn_skb_pair pairs[2];
  char data[128];
  memset(data, 'C', sizeof(data));

  int rc = kpwn_spray_skb(pairs, 2, 2, data, sizeof(data));
  if (rc != 0) {
    log_error("[smoke] skb_spray failed: %d", rc);
    return 1;
  }

  // Read one message back
  char buf[128] = {0};
  ssize_t n = recv(pairs[0].fd[1], buf, sizeof(buf), MSG_DONTWAIT);
  if (n != sizeof(data) || memcmp(buf, data, sizeof(data)) != 0) {
    log_error("[smoke] skb recv mismatch! n=%zd", n);
    kpwn_free_skb(pairs, 2);
    return 1;
  }

  kpwn_free_skb(pairs, 2);
  log_success("[smoke] skb spray+recv+free: OK");
  return 0;
}

static int test_key_spray(void) {
  int keyids[4];
  char data[64];
  memset(data, 'D', sizeof(data));

  int rc = kpwn_spray_key(keyids, 4, data, sizeof(data));
  if (rc != 0) {
    log_error("[smoke] key_spray failed: %d", rc);
    return 1;
  }

  kpwn_free_key(keyids, 4);
  log_success("[smoke] key spray+revoke: OK");
  return 0;
}

static int test_crosscache_drain(void) {
  void *maps[16];
  int rc = kpwn_drain_pages(maps, 16);
  if (rc != 0) {
    log_error("[smoke] drain_pages failed: %d", rc);
    return 1;
  }

  kpwn_release_pages(maps, 16);
  log_success("[smoke] crosscache drain+release: OK");
  return 0;
}

int main(void) {
  log_level = LOG_DEBUG;
  int fail = 0;

  log_info("[smoke] === spray smoke tests ===");

  fail += test_msg_spray();
  fail += test_pipe_spray();
  fail += test_skb_spray();
  fail += test_key_spray();
  fail += test_crosscache_drain();

  log_info("[smoke] === results: %d failures ===", fail);
  return fail;
}
