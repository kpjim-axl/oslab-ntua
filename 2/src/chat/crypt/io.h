#ifndef IO_H__
#define IO_H__

#include <crypto/cryptodev.h>

#define BLOCK_SIZE 16
#define KEY_SIZE 16

#ifndef MSG_SIZE
#define MSG_SIZE 256
#endif


struct cryptodev_ctx
{
    int cfd;
    struct session_op sess;
};

ssize_t insist_write(int fd, const void *buf, size_t cnt, struct cryptodev_ctx *ctx);
ssize_t insist_read(int fd, void *buf, size_t cnt, struct cryptodev_ctx *ctx);

int crypt_session_init(struct cryptodev_ctx *ctx, int cfd, const uint8_t *key);
void crypt_kill_session(struct cryptodev_ctx *ctx);
void getKey(uint8_t *key);

#endif