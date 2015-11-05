#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

#include "io.h"
#include "chat.h"

int main(int argc, char **argv){
    
    const char usage[] = "Usage:  for server: _chat [-p PORT] crypto_dev\n\tfor client: _chat [-p PORT] hostname cryptodev\n";
    int port = PORT;
    
    int cfd;
    uint8_t key[KEY_SIZE];
    struct cryptodev_ctx ctx;
    
    if ((cfd = open(argv[argc-1], O_RDWR)) < 0){
	perror("open");
	exit(EXIT_FAILURE);
    }
    
    /* start a new cryptodev session */
    getKey(key);
    int i;
    for (i=0; i<KEY_SIZE; i++)
	printf("%x", key[i]);
    printf("\n");
    crypt_session_init(&ctx, cfd, key);

    /* port option is set */
    if (argc == 4 || argc == 5){
	if (strcmp(argv[1], "-p")){
	    fprintf(stderr, usage);
	    crypt_kill_session(&ctx);
	    return -1;
	}
	sscanf(argv[2], "%d", &port);
	printf("connection port is %d\n", port);
    }
    
    if (argc == 3 || argc == 5)
	client(port, argv[argc-2], &ctx);
    
    else if (argc == 2 || argc == 4)
	server(port, &ctx);
    
    else
	fprintf(stderr, usage);
    
    /* end the cryptodev session */
    crypt_kill_session(&ctx);

    return 0;
}
