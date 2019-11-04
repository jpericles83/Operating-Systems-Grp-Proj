/*
 *
 * Manveer Sidhu
 * Joseph Moreno
 * Joshua Pericles
 * Savannah Pate
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

#define SHMKEY ((key_t) 7890)
#define PORTNUM  1076 /* the port number the server will listen to*/
#define DEFAULT_PROTOCOL 0  /*constant for default protocol*/

void doprocessing(int sock, int waiting);
void game_to_string(char *game_string);

enum Stages {

	AWAITING_CLIENTS, // Waiting for at least 2 clients to be connected and ready
	PLAYING, // The game has started and any new client connections must wait for it to be over
	FINISHED
};

typedef struct {
	enum Stages stage;
	int num_clients;
	int num_ready;
	int m0[16];	// m0 <-- Array of points corresponding to the letters in m1.
	char m1[16];	// m1 <-- Array of letters, a through p.

	int temp;	// Temporary variable for submission 2.
} shared_mem;

shared_mem *client_data;

int main(int argc, char *argv[])  {
	int sockfd, newsockfd, portno, clilen;
   	char buffer[256];
   	struct sockaddr_in serv_addr, cli_addr;
   	int status, pid, pid2;

   	/* First call to socket() function */
   	sockfd = socket(AF_INET, SOCK_STREAM,DEFAULT_PROTOCOL);

   
   	if (sockfd < 0) {
      		perror("ERROR opening socket");
      		exit(1);
   	}

   	/* Initialize socket structure */
   	bzero((char *) &serv_addr, sizeof(serv_addr));
   	portno = PORTNUM;
   
   	serv_addr.sin_family = AF_INET;
   	serv_addr.sin_addr.s_addr = INADDR_ANY;
   	serv_addr.sin_port = htons(portno);
   
   	/* Now bind the host address using bind() call.*/

   	status =  bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)); 

   	if (status < 0) {
      		perror("ERROR on binding");
      		exit(1);
   	}

	// Shared memory code...
	key_t key = 123;
        int shmid;
        char *shmadd;
        shmadd = (char *) 0;

        if((shmid = shmget (SHMKEY, sizeof(int), IPC_CREAT | 0660)) < 0) {
                perror("shmget");
                exit(1);
        }

        if((client_data = (shared_mem *) shmat (shmid, shmadd, 0)) == (shared_mem *)-1) {
                perror("shmat");
                exit(0);
        }

	// Initialize client_data.
	client_data->stage = AWAITING_CLIENTS;
	client_data->num_clients = 0;
	client_data->num_ready = 0;

	client_data->temp = 0;	// Temporary variable for submission 2.

	int x; char c; c = 'a';

	srand(time(0));	// Used to generate random points.

	for(x = 0; x < 16; ++x) {
		client_data->m0[x] = ((rand() % 51) - 25);	// Range of points = [-25, 25]
		client_data->m1[x] = c++;

		// Uncomment this printf(...) to check letters and their points.
		printf("%c = %d\n", client_data->m1[x], client_data->m0[x]);
	}

   	/* Now Server starts listening clients wanting to connect. No more than 5 clients allowed */
   
   	listen(sockfd, 5);
   	clilen = sizeof(cli_addr);
	
	printf("Server successfully binded on port %d. Listening for clients.\n", PORTNUM);	

	pid2 = fork(); // the game process
	if(pid2 == fork()) { 
		while(1) { 
			if(client_data->stage == AWAITING_CLIENTS && client_data->num_clients >= 2 && client_data->num_ready == client_data->num_clients) {
				client_data->stage = PLAYING;
				char game_string[32];
				game_to_string(game_string);
				printf("%s", game_string);
				client_data->stage = FINISHED;
				break;
			}
		}

		while(client_data->temp < 6);	// Temporary so that main doesn't exit until two clients give three responses each.
		
		if((shmctl(shmid, IPC_RMID, (struct shmid_ds *) 0)) == -1) {
			perror("shmctl");
			exit(-1);
		}
		
		//shutdown(sockfd, SHUT_RDWR);
		exit(0);
	}

   	while (1) {

      		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
	
		if (newsockfd < 0) {
         		perror("ERROR on accept");
         		exit(1);
      		}
      
      		/* Create child process */
      		pid = fork();
		
      		if (pid < 0) {
         		perror("ERROR on fork");
         		exit(1);
      		}

		if(pid > 0) {
			client_data->num_clients++;
			printf("Client connected. Number of connected clients: %d\n", client_data->num_clients);      
		}

      		if (pid == 0) {
         		/* This is the client process */
         		close(sockfd);
         		int waiting = client_data->stage /*== 1*/;
			doprocessing(newsockfd, waiting);
         		exit(0);
      		} else {
         		close(newsockfd);
      		}
   	} /* end of while */
}


void doprocessing (int sock, int waiting) {
	write(sock, (const char *) &(client_data->stage), 4); // tell client which stage we're currently on
	while(1) {	
		int status;
   		char buffer[256];
   		bzero(buffer, 256);
		
		if(client_data->stage == AWAITING_CLIENTS) {
			int prev = client_data->stage;
			
			if(waiting == 1) { // put any clients that are waiting back on track
				waiting = 0;
				write(sock, (const char *) &(client_data->stage), 4);
			}
			
			read(sock, buffer, 255);
			client_data->num_ready++;;
			//printf("Ready: %d\n", client_data->num_ready);	
			
			while(prev == client_data->stage);
			
			char game_string[32];
			int t;	// Temporary variable used for submission 2.

			// while(client_data->stage == PLAYING)						}
			// OR										} Loops for final submission.
			// for(client_data->count = 0; client_data->count < 16; ++(client_data->count)) }
			for(t = 0; t < 3; ++t) {	// Temporary loop for submission 2.
				bzero(game_string, 32);
				game_to_string(game_string);
				status = write(sock, game_string, 32);	// Print matrix to client.

				bzero(buffer, 256);
				read(sock, buffer, 255);	// Read in client's choice.
				
				switch(buffer[0]) {
					case 'A':
					case 'a': printf("%c = %d\n", client_data->m1[0], client_data->m0[0]);
						  break;
					case 'B':
                                        case 'b': printf("%c = %d\n", client_data->m1[1], client_data->m0[1]);
                                                  break;
					case 'C':
                                        case 'c': printf("%c = %d\n", client_data->m1[2], client_data->m0[2]);
                                                  break;
                                        case 'D':
                                        case 'd': printf("%c = %d\n", client_data->m1[3], client_data->m0[3]);
                                                  break;
					case 'E':
                                        case 'e': printf("%c = %d\n", client_data->m1[4], client_data->m0[4]);
                                                  break;
                                        case 'F':
                                        case 'f': printf("%c = %d\n", client_data->m1[5], client_data->m0[5]);
                                                  break;
                                        case 'G':
                                        case 'g': printf("%c = %d\n", client_data->m1[6], client_data->m0[6]);
                                                  break;
                                        case 'H':
                                        case 'h': printf("%c = %d\n", client_data->m1[7], client_data->m0[7]);
                                                  break;
					case 'I':
                                        case 'i': printf("%c = %d\n", client_data->m1[8], client_data->m0[8]);
                                                  break;
                                        case 'J':
                                        case 'j': printf("%c = %d\n", client_data->m1[9], client_data->m0[9]);
                                                  break;
                                        case 'K':
                                        case 'k': printf("%c = %d\n", client_data->m1[10], client_data->m0[10]);
                                                  break;
                                        case 'L':
                                        case 'l': printf("%c = %d\n", client_data->m1[11], client_data->m0[11]);
                                                  break;
                                        case 'M':
                                        case 'm': printf("%c = %d\n", client_data->m1[12], client_data->m0[12]);
                                                  break;
                                        case 'N':
                                        case 'n': printf("%c = %d\n", client_data->m1[13], client_data->m0[13]);
                                                  break;
                                        case 'O':
                                        case 'o': printf("%c = %d\n", client_data->m1[14], client_data->m0[14]);
                                                  break;
                                        case 'P':
                                        case 'p': printf("%c = %d\n", client_data->m1[15], client_data->m0[15]);
                                                  break;
					default: printf("Invalid choice received from client.\n");
				} // Switch statement end.

				client_data->temp += 1;
			} // For-loop end.
			
			break;
		}
	}
	
}

void game_to_string(char *game_string) {
	int i = 0;

        while (i < 16) {
                strncat(game_string, &(client_data->m1[i]), 1);
                if(i % 4 == 3)
                	strcat(game_string, "\n");
                else
                        strcat(game_string, "\t");
                i++;
	}
}
