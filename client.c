/*
 *
 * Manveer Sidhu
 * Joseph Moreno
 * Joshua Pericles
 * Savannah Pate
 *
 */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>

#define PORTNUM  7777 /* the port number that the server is listening to*/
#define DEFAULT_PROTOCOL 0  /*constant for default protocol*/

int stage = 0;

void main(int argc, char **argv) {
	int  port;
  	int  socketid;      /*will hold the id of the socket created*/
   	int  status;        /* error status holder*/
   	char buffer[256];   /* the message buffer*/
   	struct sockaddr_in serv_addr;

   	struct hostent *server;

	char host[8];
	strcat(host, "osnode");
	strcat(host, argv[1]);

   	/* this creates the socket*/
   	socketid = socket (AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);
   	if (socketid < 0) {
      		printf( "error in creating client socket\n"); 
      		exit (-1);
    	}

    	printf("Created client socket successfully\n");

   	/* before connecting the socket we need to set up the right values in the different fields of the structure server_addr 
   	you can check the definition of this structure on your own*/
   
    	server = gethostbyname(host); 

   	if (server == NULL) {
      		printf("Error trying to identify the machine where the server is running\n");
      		exit(0);
   	}

   	port = PORTNUM;
	/*This function is used to initialize the socket structures with null values. */
   	bzero((char *) &serv_addr, sizeof(serv_addr));

   	serv_addr.sin_family = AF_INET;
   	bcopy((char *)server->h_addr,
        	(char *)&serv_addr.sin_addr.s_addr,
        		server->h_length); 
  	serv_addr.sin_port = htons(port);
   
   	/* connecting the client socket to the server socket */

   	status = connect(socketid, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
   	
	if (status < 0) {
      		printf("Error in connecting client socket with server	\n");
      		exit(-1);
    	}

   	printf("Connected client socket to the server socket \n");

	status = read(socketid, &stage, 4); // get current stage of the game
	if(stage == 1) {
		printf("There is currently a game in session on this host. Please wait for it to finish.\n");
		status = read(socketid, &stage, 4); // wait until server sends ready signal
	}
   	/* now lets send a message to the server. the message will be
     	whatever the user wants to write to the server.*/
  	while(1) {
		if(stage == 0) { // if game is waiting on clients
   			printf("Please enter your name to begin playing: ");
   			bzero(buffer, 256);
   			fgets(buffer, 256, stdin);
   			status = write(socketid, buffer, strlen(buffer));

			printf("Please wait...\n");		

	   		if (status < 0) {
     				printf("error while sending client message to server\n");
   			}
   
			/* Read server response */
   			bzero(buffer, 256);
   			status = read(socketid, buffer, 256);
   
   			/* Upon successful completion, read() returns the number 
   			of bytes actually read from the file associated with fields.
   			This number is never greater than nbyte. Otherwise, -1 is returned. */
   			if (status < 0) {
      				perror("error while reading message from server");
      				exit(1);
   			}
   
   			printf("%s\n", buffer);
			stage = 1;
		} else if(stage == 1) { // if the game has started
			while(stage == 1) {
				printf("Please enter a location: ");
			
				char location = getchar(); 
				int remaining; // getchar() returns unsigned char cast to an int or EOF
				while((remaining = getchar()) != '\n' && remaining != EOF); // grab the first character and purge the input buffer
				status = write(socketid, (const char *) &location, 1);

				bzero(buffer, 256);
				status = read(socketid, buffer, 256);
				printf("\n%s\n", buffer);

				status = read(socketid, &stage, 4); // make sure game is still in session
			}
		} else if(stage == 2) // finished
			break;

	}
   	/* this closes the socket*/
   	close(socketid);

  
} 

