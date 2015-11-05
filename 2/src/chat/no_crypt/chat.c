#include <stdio.h>
#include <string.h>

#include "chat.h"

// void client(int port){
//     printf("client\n");
// }
// void server(int port){
//     printf("server\n");
// }
/*
int main(int argc, char **argv){
    int err = 0;
    int port;
    if (argc == 2 || argc == 4){
	if (argc == 4)
	    if (strcmp(argv[1], "-p"))
		err = 1;
	    else
		sscanf(argv[2], "%d", &port);
	if (!err)
	    client(port);
    }
    else if (argc == 1 || argc == 3){
	if (argc == 3)
	    if (strcmp(argv[1], "-p"))
		err = 1;
	if (!err)
	    server(port);
    }
    else
	err = 1;
    
    if (err)
	fprintf(stderr, "Usage:  for server: BLABLA [-p PORT]\n\tfor client: BLABLA [-p PORT] hostname\n");
    return 0;
}*/

int main(int argc, char **argv){
    const char usage[] = "Usage:  for server: _chat [-p PORT]\n\tfor client: _chat [-p PORT] hostname\n";
    int port = PORT;
    
    /* port option is set */
    if (argc == 3 || argc == 4){
	if (strcmp(argv[1], "-p")){
	    fprintf(stderr, usage);
	    return -1;
	}
	sscanf(argv[2], "%d", &port);
	printf("connection port is %d\n", port);
    }
    
    if (argc == 2 || argc == 4){
	if (argc == 2)
	    client(port, argv[1]);
	else
	    client(port, argv[3]);
    }
    else if (argc == 1 || argc == 3){
	server(port);
    }
    else
	fprintf(stderr, usage);
    
    return 0;
}
