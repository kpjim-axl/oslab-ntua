#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <crypto/cryptodev.h>
#include <fcntl.h>
#include <string.h>
// #include <time.h>
#include "io.h"


// int encrypt_buffer(struct cryptodev_ctx *ctx, const void *iv, const void *raw, void *cooked, ssize_t size);
// int decrypt_buffer(struct cryptodev_ctx *ctx, const void *iv, const void *raw, void *cooked, ssize_t size);

/* insist_{read, write}() and cryptodev calls are introduced here
 * The library will provide insist_{read, write}() functions, which are
 * to be used to send and receive data, and crypt_{new, kill}Session() functions
 * which are to be used to open or close a new cryptodev session. */


/*******************
 * READ-WRITE AREA *
 ******************/
ssize_t insist_write(int fd, const void *buf, size_t cnt, struct cryptodev_ctx *ctx)
{
    void *en;
    char iv[BLOCK_SIZE];
    ssize_t ret;
    size_t orig_cnt = cnt;
    
    memset(iv, 0x0, sizeof(iv));
    en = malloc(cnt * sizeof(char));

    if (encrypt_buffer(ctx, iv, buf, en, MSG_SIZE) < 0){
	perror("encryption failed");
	return -1;
    }
    
    while (cnt > 0) {
	ret = write(fd, en, cnt);
	if (ret < 0)
	    return ret;
	en += ret;
	cnt -= ret;
    }
    
    return orig_cnt;
}

ssize_t insist_read(int fd, void *buf, size_t cnt, struct cryptodev_ctx *ctx)
{
    /* oxi akrivws insist_read alla etsi 3emeine to onoma! */
    void *en;
    char iv[BLOCK_SIZE];
    ssize_t ret;
    size_t orig_cnt = cnt;
    
    memset(iv, 0x0, sizeof(iv));
    en = malloc(cnt * sizeof(char));
    
//     while (cnt > 0) {
// 	ret = read(fd, en, cnt);
// 	if (ret < 0)
// 	    return ret;
// 	en += ret;
// 	cnt -= ret;
//     }
    
    orig_cnt = read(fd, en, cnt);
    
    if (decrypt_buffer(ctx, iv, en, buf, MSG_SIZE) < 0){
	perror("decryption failed");
	return -1;
    }
    
    
    return orig_cnt;
}


/******************
 * CRYPTODEV AREA * 
 *****************/
/* internal functions for encrypting or decrypting data */
int encrypt_buffer(struct cryptodev_ctx *ctx, const void *iv, const void *raw, void *cooked, ssize_t size){
    struct crypt_op cryp;
    
    memset(&cryp, 0, sizeof(cryp));
    cryp.ses = ctx->sess.ses;
    cryp.len = size;
    cryp.src = (void *) raw;
    cryp.dst = cooked;
    cryp.iv = (void *) iv;
    cryp.op = COP_ENCRYPT;
    
    if (ioctl(ctx->cfd, CIOCCRYPT, &cryp)){
	perror("ioctl(CIOCCRYPT)");
	return -1;
    }
    
    return 0;
}

int decrypt_buffer(struct cryptodev_ctx *ctx, const void *iv, const void *raw, void *cooked, ssize_t size){
    struct crypt_op cryp;
    
    memset(&cryp, 0, sizeof(cryp));
    cryp.ses = ctx->sess.ses;
    cryp.len = size;
    cryp.src = (void *)raw;
    cryp.dst = cooked;
    cryp.iv = (void *) iv;
    cryp.op = COP_DECRYPT;
    
    if (ioctl(ctx->cfd, CIOCCRYPT, &cryp)){
	perror("ioctl(CIOCCRYPT)");
	return -1;
    }
    
    return 0;
}

/* create or delete a session */
int crypt_session_init(struct cryptodev_ctx *ctx, int cfd, const uint8_t *key){
    
    memset(ctx, 0, sizeof(*ctx));
    
    ctx->cfd = cfd;
    
    ctx->sess.key = (void *) key;
    ctx->sess.keylen = KEY_SIZE;
    ctx->sess.cipher = CRYPTO_AES_CBC;
    
    if (ioctl(ctx->cfd, CIOCGSESSION, &ctx->sess)) {
	perror("ioctl(CIOCGSESSION)");
	return -1;
    }
    
    return 0;
}

void crypt_kill_session(struct cryptodev_ctx* ctx) 
{
    if (ioctl(ctx->cfd, CIOCFSESSION, &ctx->sess.ses)) {
	perror("ioctl(CIOCFSESSION)");
    }
}

void getKey(uint8_t *key){
    int i;
    uint8_t d;
//     srand(time(NULL));
    for (i=0; i<KEY_SIZE; i++){
// 	d = rand();
	d = i % 0xff;
	memset(key + i, d, sizeof(uint8_t));
    }
}
#define M 12
#ifndef M
int main(){
    
    char message[M] = {'r', 'e', 'a', 'l', ' ', 'm', 'a', 'd', 'r', 'i', 'd', '\0' };
    char mes[M], m[M], w[M];
    struct cryptodev_ctx ctx;
    uint8_t key[KEY_SIZE];
    char iv[BLOCK_SIZE];
    int cfd;
    int bla;
    
    cfd = open("/dev/crypto", O_RDWR);
    bla = open("./test", O_WRONLY | O_CREAT);
    
    getKey(key);
    memset(iv, 0x0, sizeof(iv));

    crypt_session_init(&ctx, cfd, key);
    
    

    encrypt_buffer(&ctx, iv, message, mes, BLOCK_SIZE);
//     decrypt_buffer(&ctx, iv, mes, m, BLOCK_SIZE);
    
//     printf("bla: %s\n\n\n\n", m);
    
    write(bla, mes, M);
    
    close(bla);
    bla = open("./test", O_RDONLY);
    
    read(bla, m, M);
    
//     insist_read(bla, mes, M, &ctx);
//     mes[M-1] = '\0';
    
    decrypt_buffer(&ctx, iv, m, w, BLOCK_SIZE);
    
    printf("\n%s\n", w);
    
    crypt_kill_session(&ctx);
    close(cfd);
    close(bla);
    return 0;
}

#endif