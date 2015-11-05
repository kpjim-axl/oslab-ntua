/* File: server.c
 * Purpose: chat process for oslab course ('server' side!)
 * Author: Konstantinos Papadimitriou
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "chat.h"
#include "io.h"

char msg[MSG_SIZE + 1];
char kb_msg[MSG_SIZE + 1];

void exitClient(int fd, fd_set *readfds, int fd_array[], int *num_clients){
    int i;
    
    close(fd);
    FD_CLR(fd, readfds); //clear the leaving client from the set
    for (i = 0; i < (*num_clients) - 1; i++)
        if (fd_array[i] == fd)
            break;          
    for (; i < (*num_clients) - 1; i++)
        (fd_array[i]) = (fd_array[i + 1]);
    (*num_clients)--;
}

void server(int port, struct cryptodev_ctx *ctx){
    /* declarations */
    int sd, fd, newsd;
    int fd_array[MAX_CLIENTS];
    int clients = 0, i, r, to;
    struct sockaddr_in sa;
    fd_set readfds, t;
    
    printf("Server process starting (port: %d). For Help type \"-2 help\"\n", port);
    
    /* socket */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
	perror("socket");
	exit(EXIT_FAILURE);
    }
    
    /* bind */
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sd, (struct sockaddr *)&sa, sizeof(sa)) < 0){
	perror("bind");
	exit(EXIT_FAILURE);
    }
    
    /* listen */
    if (listen(sd, TCP_BACKLOG) < 0){
	perror("listen");
	exit(EXIT_FAILURE);
    }
    
    /* and now the rest!! */
    FD_ZERO(&readfds);
    FD_SET(sd, &readfds);	// add sd to readfds
    FD_SET(0, &readfds);	// add keyboard to readfds
    
    for(;;){
	t = readfds;
	select(FD_SETSIZE, &t, NULL, NULL, NULL);
	
	for(fd=0; fd<FD_SETSIZE; fd++){
	    if (FD_ISSET(fd, &t)){
		if (fd == 0){
		    /* KEYBOARD ACTION 
		     * Η αποστολή των μηνυμάτων από τον server γίνεται ως εξής:
		     * * fd message<enter>
		     * οπότε το message θα αποσταλλεί μόνο στον client με file descriptor fd
		     * 
		     * Αν το fd είναι 0, το message θα αποσταλλεί σε όλους τους clients
		     * 
		     * Αν το fd είναι -1 τότε ο server τερματίζει τη λειτουργία του.
		     *
		     * Αν το fd είναι -2, τότε ο server θα προσπαθήσει να εξυπηρετήσει την
		     * αίτηση (command) του message (χωρίς να το στείλει σε κάποιον).
		     * Συγγεκριμένα αν message είναι "help" θα τυπωθεί
		     * ένα μήνυμα βοήθειας στην οθόνη, αν το message έιναι "show" τότε θα εκτυπωθούν οι
		     * file descriptors όλων των client (ίσως με τη διεύθυνσή τους?) και τέλος αν το message
		     * είναι "quit" το server θα τερματίσει τη λείτουργία του.
		     * 
		     * Τέλος ο server μπορεί να εξυπηρετήσει και αιτήσεις των client. Ο τρόπος εξηγείται παρακάτω
		     * καθώς επίσης και στο client.c
		     */
		    scanf("%d",&to);
		    if (to != -1)
			fgets(kb_msg, MSG_SIZE, stdin);

		    switch (to){
			case 0:
			{
			    /* insist_write to all! FIXME: insist leme!! */
			    sprintf(msg, "server: %s", kb_msg+1);
			    for (i = 0; i<clients; i++)
				insist_write(fd_array[i], msg, MSG_SIZE, ctx);
			    break;
			}
			
			case -1:
			{
			    /* exit server  FIXME: insist_write */
			    sprintf(msg, "Server is shutting down.\n");
			    for (i=0; i<clients; i++){
				insist_write(fd_array[i], msg, MSG_SIZE, ctx);
				close(fd_array[i]);
			    }
			    close(sd);
			    exit(0);
			    break;
			}
			
			case -2:
			{
			    /* command process */
			    if (strcmp(kb_msg+1, "help\n") == 0){
				/* help command */
				printf("blablabla\n");
			    }
			    
			    else if (strcmp(kb_msg+1, "quit\n") == 0){
				/* quit command FIXME: insist_write */
				sprintf(msg, "Server is shutting down.\n");
				for (i=0; i<clients; i++){
				    insist_write(fd_array[i], msg, MSG_SIZE, ctx);
				    close(fd_array[i]);
				}
				close(sd);
				exit(0);
			    }
			    
			    else if (strcmp(kb_msg+1, "show\n") == 0){
				/* print client_info for every client */
				for (i=0; i<clients; i++)
				    printf("client %d: %d\n", i, fd_array[i]);
				
			    }
			    
			    else
			    {
				printf("Unknown command\n");
// 				fflush(stdin); /* ? */
			    }
			    
			    break;
			}
			
			default:
			{
			    /* send message FIXME: insist_write */
			    sprintf(msg, "server: %s", kb_msg+1);
			    for (i=0; i<clients; i++)
				if (fd_array[i] == to){
				    insist_write(to, msg, MSG_SIZE, ctx);
				    break;
				}
			    if (i==clients)
				printf("Unknown client\n");
			    break;
			    /* check {to} and insist_write */
			}
		    }
		}
		
		
		else if (fd == sd){
		    /* accept or decline a new connection request */
		    if ((newsd = accept(sd, NULL, NULL)) < 0){
			perror("accept");
			exit(EXIT_FAILURE);
		    }
		    if (clients < MAX_CLIENTS){
			FD_SET(newsd, &readfds);
			fd_array[clients++] = newsd;
			fprintf(stderr, "accepted client %d (fd: %d)\n", clients, newsd);
			
			sprintf(msg, "Server: Welcome aboard!\nYou are client %d and your fd is %d\n", clients, newsd);
			insist_write(newsd, msg, MSG_SIZE, ctx);		// FIXME: insist_write
		    }
		    else {
			sprintf(msg, "Sorry, too many clients. Try again later.\n");
			insist_write(newsd, msg, MSG_SIZE, ctx);		// FIXME: insist_write
			close(newsd);
		    }
		}
		
		
		else if (fd > 0){
		    /* someone (fd) sent a message.. process data and write to 1 */
		    /* ΕΠΕΞΕΡΓΑΣΙΑ ΔΕΔΟΜΕΝΩΝ CLIENT
		     * 
		     * Όταν το μήνυμα ξεκινάει με τον χαρακτήρα '/' πρόκειται για αίτηση..
		     * Έτσι μια αίτηση έχει τη μορφή "/command".
		     * To command μπορεί να είναι είτε "show", οπότε ο server καλείται να στείλει
		     * πληροφορίες σχετικά με τους άλλους clients (client_no, fd, κλπ), είτε "quit"
		     * οπότε ο server ενημερώνεται πως ο client "την κάνει". Υπαρχει και η "help", αλλά
		     * αυτή την επεξεργάζεται και η διεργασία client.
		     * * Μελλοντικά θα προστεθεί και δυνατότητα whisper, οπότε ο ένας client θα μπορεί
		     * * να στείλει μήνυμα αποκλειστηκά σε κάποιον άλλον, ενώ χωρίς whisper το μήνυμα θα
		     * * πηγαίνει σε όλους (και όχι μόνο στον server). (Έτσι κ αλλιώς, όπως έχουν τα πράγματα
		     * * προς το παρόν, μάλλον αχρείαστη είναι η "show"!)
		     */
		    
		    if ((r = insist_read(fd, msg, MSG_SIZE, ctx)) < 0){
			perror("read");
			exit(EXIT_FAILURE);
		    }
		    if (r > 0){
			msg[r] = '\0';
			if (msg[0] == '/'){
			    /* command */
			    if (strcmp(msg+1, "quit\n") == 0){
				exitClient(fd, &readfds, fd_array, &clients);
				printf("%d left the chat\n", fd);
			    }
			    
			    else if (strcmp(msg+1, "show\n") == 0){
				for (i=0; i<clients; i++){
				    sprintf(msg, "Client: %d, fd: %d\n", i+1, fd_array[i]);
				    insist_write(fd, msg, MSG_SIZE, ctx);
				}
			    }
			    
			    else {
				sprintf(msg, "Unknown command\n");
				insist_write(fd, msg, MSG_SIZE, ctx);
			    }
			}
			
			else
			    printf("%d: %s", fd, msg);
		    }
		}
		
		else {
		    exitClient(fd, &readfds, fd_array, &clients);
		}
	    }
	}
    }
}
