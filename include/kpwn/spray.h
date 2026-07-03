#ifndef _KPWN_SPRAY_
#define _KPWN_SPRAY_

#include <stddef.h>
#include <stdint.h>

// Heap-spray primitives for kernel exploitation.
//
// Each object type provides alloc / free / write helpers. The common pattern:
//   int ids[N];
//   kpwn_spray_msg(ids, N, data, data_len, KPWN_MSG_KMALLOC_ANY);
//   ... trigger vulnerability ...
//   kpwn_free_msg(ids, N);
//
// Data sizes determine which kmalloc-* slab the objects land in.

// --- msg_msg / msg_msgseg ----------------------------------------------------
// msgsnd/msgrcv spray. MSGMAX=~8KB per msg_msg; larger payloads chain via
// msg_msgseg. The msg_msg header is 48 bytes, so usable payload per primary
// segment = size - 48. Useful for UAF/OOB in kmalloc-64 through kmalloc-4k.

#define KPWN_MSG_HDR_SIZE 48
#define KPWN_MSGSEG_HDR_SIZE 8

// Flags for msg spray target
#define KPWN_MSG_KMALLOC_ANY 0
#define KPWN_MSG_KMALLOC_CG 1

int kpwn_spray_msg(int *qids, size_t n, const void *data, size_t data_len,
                   int flags);
int kpwn_free_msg(int *qids, size_t n);
int kpwn_read_msg(int qid, void *buf, size_t buf_len);

// --- pipe_buffer (struct pipe_buffer spray via pipe) -------------------------
// Each pipe page has a 40-byte pipe_buffer struct in the pipe_inode_info ring.
// Spray by creating pipes and writing to them; release by close().

struct kpwn_pipe {
  int fd[2]; // [0]=read, [1]=write
};

int kpwn_spray_pipe(struct kpwn_pipe *pipes, size_t n);
int kpwn_write_pipe(struct kpwn_pipe *pipes, size_t n, const void *data,
                    size_t data_len);
int kpwn_free_pipe(struct kpwn_pipe *pipes, size_t n);

// --- setxattr ----------------------------------------------------------------
// setxattr allocates an arbitrary-size kmalloc buffer, copies user data, then
// frees on failure (if the filesystem doesn't support xattr). Useful for
// "allocate-then-free" patterns and grooming.

int kpwn_spray_xattr(const char *path, size_t n, const void *data,
                     size_t data_len);

// --- add_key (user_key_payload) ----------------------------------------------
// request_key / add_key allocates a user_key_payload of arbitrary size in the
// kernel keyring. The payload persists until the key is revoked/unlinked.
// Header is 24 bytes; usable = alloc_size - 24.

#define KPWN_KEY_HDR_SIZE 24

int kpwn_spray_key(int *keyids, size_t n, const void *data, size_t data_len);
int kpwn_free_key(int *keyids, size_t n);

// --- sk_buff (via socketpair) ------------------------------------------------
// sendmsg on a AF_UNIX socketpair sprays sk_buff + linear data. Data includes
// a ~320-byte skb_shared_info trailer. Controlled size and content.

#define KPWN_SKB_SHARED_INFO_SIZE 320

struct kpwn_skb_pair {
  int fd[2];
};

int kpwn_spray_skb(struct kpwn_skb_pair *pairs, size_t n_pairs,
                   size_t msgs_per_pair, const void *data, size_t data_len);
int kpwn_free_skb(struct kpwn_skb_pair *pairs, size_t n_pairs);

// --- pgv (packet socket page vector spray) -----------------------------------
// AF_PACKET + PACKET_VERSION + setsockopt(PACKET_TX_RING/PACKET_RX_RING)
// allocates compound pages of a controlled order. Used for page-level UAF and
// cross-cache attacks.

int kpwn_spray_pgv(int *fds, size_t n, unsigned int block_size,
                   unsigned int block_nr, unsigned int frame_size);
int kpwn_free_pgv(int *fds, size_t n);

#endif
