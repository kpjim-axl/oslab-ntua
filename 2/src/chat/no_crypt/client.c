#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <unistd.h>

#include "chat.h"

void client(int port, /* const */ char *hostname){
    
    char msg[MSG_SIZE + 1];
    char kb_msg[MSG_SIZE + 10];
    int r;
    int sd;
    struct hostent *hp;
    struct sockaddr_in sa;
    fd_set readfds, t;
    
    printf("Client process starting (port: %d). For Help type /help<enter>\n", port);
    
    /* create a socket */
    if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0){
	perror("socket");
	exit(EXIT_FAILURE);
    }
    
    /* get host info */

    if ( !(hp = gethostbyname(hostname))) {
	fprintf(stderr, "DNS failed lookup for host: %s\n", hostname);
	exit(EXIT_FAILURE);
    }
    
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    memcpy(&sa.sin_addr.s_addr, hp->h_addr, sizeof(struct in_addr));
    if (connect(sd, (struct sockaddr *) &sa, sizeof(sa)) < 0){
	perror("connect");
	exit(EXIT_FAILURE);
    }
    
    /* put keyboard and sd into the readfds set */
    FD_ZERO(&readfds);
    FD_SET(0, &readfds);
    FD_SET(sd, &readfds);
    
    /* loop until user gives "quit\n" */
    for (;;){
	t = readfds;
	select(FD_SETSIZE, &t, NULL, NULL, NULL);
	
	if (FD_ISSET(0, &t)){
	    /* keyboard action.. get data and send */
	    fgets(kb_msg, MSG_SIZE, stdin);
	    
	    if (strcmp(kb_msg, "/help\n") == 0)
		printf("Type the message you want to send and press <enter>\n\
			Commands: /help -- Print this message\n\
			          /show -- Print info for every client connected to the server\n\
			          /quit -- Quit\n");
	    else {
		/* insist_write on sd */
		write(sd, kb_msg, strlen(kb_msg));
		
		if (strcmp(kb_msg, "/quit\n") == 0){
		    printf("Going Home!\n");
		    close(sd);
		    exit(EXIT_SUCCESS);
		}
	    }
	}
	
	if (FD_ISSET(sd, &t)){
	    /* server sent a message.. process data and print */
	    if ((r = read(sd, msg, MSG_SIZE)) < 0){
		perror("read");
		exit(EXIT_FAILURE);
	    }
	    msg[r] = '\0';
	    
	    printf("%s", msg);
	}
    }
}
