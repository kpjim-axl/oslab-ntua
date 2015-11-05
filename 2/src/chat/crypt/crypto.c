#include <crypto/cryptodev.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>

#include <sys/ioctl.h>
 
#include <sys/types.h>
#include <sys/stat.h>


#define DATA_SIZE       256
#define BLOCK_SIZE      16
#define KEY_SIZE	16  /* AES128 */

struct cryptodev_ctx
{
    int cfd;
    struct session_op sess;
};

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

int encrypt_buffer(struct cryptodev_ctx *ctx, const void *iv, const void *raw, void *cooked, ssize_t size){
    struct crypt_op cryp;
    
    memset(&cryp, 0, sizeof(cryp));
    cryp.ses = ctx->sess.ses;
    cryp.len = DATA_SIZE;
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
    cryp.len = DATA_SIZE;
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

ssize_t insist_read(int fd, void *buf, size_t cnt)
{
        ssize_t ret;
        size_t orig_cnt = cnt;

        while (cnt > 0) {
                ret = read(fd, buf, cnt);
                if (ret < 0)
                        return ret;
                buf += ret;
                cnt -= ret;
        }

        return orig_cnt;
}

void get_theKey(uint8_t *key){
    int i;
    uint8_t d;
    for (i=0; i<KEY_SIZE; i++){
	d = i % 0xff;
	memset(key + i, d, sizeof(uint8_t));
    }
//     for (i=0; i<N;
//     key = { 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
}

int main(){
    char message[DATA_SIZE], en[DATA_SIZE], dec[DATA_SIZE];
    uint8_t key[KEY_SIZE];
    char iv[BLOCK_SIZE];
    int cfd;
    
    struct cryptodev_ctx ctx;
    
    
    
    if ((cfd = open("/dev/crypto", O_RDWR)) < 0){
	perror("open");
	exit(EXIT_FAILURE);
    }
    get_theKey(key);
    crypt_session_init(&ctx, cfd, key);
    
    memset(iv, 0x0, sizeof(iv));
    
    
//     if (insist_read(0, key, KEY_SIZE) < 0){
// 	perror("insist_read\n");
// 	exit(EXIT_FAILURE);
//     }
    
//     memset(key, 0, KEY_SIZE);
    int i=0; char c;
    
//     while (((c = fgetc(stdin)) != '\n') && i < KEY_SIZE)
// 	message[i++] = c;
    
//     i = 0;
    
    while ((c = fgetc(stdin)) != '\n')
	message[i++] = c;
    
    message[i] = '\0';
//     insist_read(0, message, DATA_SIZE);
    printf("RAW:\n%s\n\n", message);
    
    if (encrypt_buffer(&ctx, iv, message, en, BLOCK_SIZE) < 0){
	perror("encrypt_buffer");
	exit(EXIT_FAILURE);
    }
    
    printf("EN:\n%s\n\n", en);
    
    if (decrypt_buffer(&ctx, iv, en, en, BLOCK_SIZE) < 0){
	perror("decrypt_buffer");
	exit(EXIT_FAILURE);
    }
    
    printf("DEC:\n%s\n\n", en);
    
    
    crypt_kill_session(&ctx);
    return 0;
}
    

ssize_t insist_write(int fd, const void *buf, size_t cnt)
{
	ssize_t ret;
	size_t orig_cnt = cnt;
	
	/* crypt buf */
	
	
	while (cnt > 0) {
	        ret = write(fd, buf, cnt);
	        if (ret < 0)
	                return ret;
	        buf += ret;
	        cnt -= ret;
	}

	return orig_cnt;
}

