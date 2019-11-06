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
#include<time.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<netinet/in.h>
#include<netdb.h>
#include<unistd.h>

#define SHMKEY ((key_t) 7890)
#define PORTNUM  7777 /* the port number the server will listen to*/
#define DEFAULT_PROTOCOL 0  /*constant for default protocol*/

void doprocessing(int sock, int waiting);
void game_to_string(char *game_string);
void generate_game_matrix();
int select_location(char location, char *name);

enum Stages {

	AWAITING_CLIENTS, // Waiting for at least 2 clients to be connected and ready
	PLAYING, // The game has started and any new client connections must wait for it to be over
	FINISHED // Game is over (replay prompt)
};

typedef struct {
	enum Stages stage;
	int num_clients;
	int num_ready;
	int game_matrix[16];
	int locations_left;
} shared_mem;
shared_mem *client_data;

char name[20];
int points;

int main(int argc, char *argv[])  {
	int sockfd, newsockfd, portno, clilen;
   	char buffer[256];
   	struct sockaddr_in serv_addr, cli_addr;
   	int status, pid, pid2;
	
	// Shared memory stuff
	key_t key = 123;   
	int shmid;
	char *shmadd;
	shmadd = (char *) 0;


	if((shmid = shmget (SHMKEY, sizeof(int), IPC_CREAT | 0666)) < 0) {
		perror("shmget");
		exit(1);
	}

	if((client_data = (shared_mem *) shmat (shmid, shmadd, 0)) == (shared_mem *)-1) {
		perror("shmat");
		exit(0);
	}

	// initialize
	client_data->stage = AWAITING_CLIENTS;
	client_data->num_clients = 0;
	client_data->num_ready = 0;
	client_data->locations_left = 0;
	srand(time(NULL));

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
   
   	/* Now Server starts listening clients wanting to connect. No more than 5 clients allowed */
   
   	listen(sockfd, 5);
   	clilen = sizeof(cli_addr);
	
	printf("Server successfully binded on port %d. Listening for clients.\n", PORTNUM);

	pid2 = fork(); // the game process
	if(pid2 == fork()) { 
		while(1) {
			if(client_data->stage == AWAITING_CLIENTS && client_data->num_clients >= 2 && client_data->num_ready == client_data->num_clients) {
				generate_game_matrix();
				char game_string[32];
				game_to_string(game_string);
				printf("%s\n", game_string);
				client_data->stage = PLAYING;
			} else if(client_data->stage == PLAYING && client_data->locations_left == 0) {
				printf("All game locations have been selected.\n");
				client_data->stage = FINISHED;
			}
		}
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
         		int waiting = client_data->stage == 1;
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
		
		if(client_data->stage == AWAITING_CLIENTS) {
			if(waiting == 1) { // put any clients that are waiting back on track
				waiting = 0;
				write(sock, (const char *) &(client_data->stage), 4);
			}
			read(sock, buffer, 256);
			int i = 0;
			while(i < 20) { // read first 20 characters, or until the first white space/new line
				if(buffer[i] == ' ' || buffer[i] == '\n')
					break;
				name[i] = buffer[i];
				i++;
			}
			client_data->num_ready++;;
			while(client_data->stage == AWAITING_CLIENTS);
			bzero(buffer, 256);
			game_to_string(buffer);
			status = write(sock, buffer, 256);
		} else if(client_data->stage == PLAYING) {
			if(waiting == 1) // ignore any clients that are waiting for now
				continue;
			while(client_data->locations_left > 0) {
				char location;
				read(sock, &location, 1);
				int pts = select_location(location, name);
				bzero(buffer, 256);
				if(pts > -10)
					snprintf(buffer, sizeof(buffer), "You receive %d points for selecting location %c, and now have a total of %d points.\n", pts, location, points);
				else if(pts == -10)
					snprintf(buffer, sizeof(buffer), "Location '%c' has already been claimed.\n", location);
				else if(pts == -20)
					snprintf(buffer, sizeof(buffer), "Invalid location '%c'.\n", location);
				else if(pts == -30)
					snprintf(buffer, sizeof(buffer), "This game has finished. Your score was: %d.\n", points);
				game_to_string(buffer);
				status = write(sock, buffer, strlen(buffer));
				status = write(sock, (const char *) &(client_data->stage), 4); // let client exit this game if it has expired
			}
		} else if(client_data->stage == FINISHED) {
			status = write(sock, (const char *) &(client_data->stage), 4);
			break;
		}
	}
	
}

void game_to_string(char *game_string) {
	int i = 0;
        while(i < 16) {
        	char c = client_data->game_matrix[i] == -50 ? '-' : 'a' + i;
                strncat(game_string, &c, 1);
                if(i % 4 == 3)
                	strcat(game_string, "\n");
                else
                        strcat(game_string, "\t");
                i++;
	}
}

void generate_game_matrix() {
	int i = 0;
	while(i < 16) {
		int r1 = rand() % 5, r2;
		if(r1 == 3) // 20% chance of being a negative number
			r2 = -(rand() % 10);
		else 
			r2 = rand() % 20;
		client_data->game_matrix[i] = r2;
		i++;
	}
	client_data->locations_left = 16;
}

int select_location(char location, char *name) {
	if(client_data->stage == FINISHED)
		return -30;

	if(location < 97 || location > 112)
		return -20;

	int index = location - 97;
	if(client_data->game_matrix[index] == -50)
		return -10;

	int location_value = client_data->game_matrix[index];
	points += location_value;
	printf("%s selected location '%c' for %d points, and now has a total of %d points.\n\n", name, location, location_value, points);
	client_data->game_matrix[index] = -50;
	client_data->locations_left--;
	char game_string[50] = "";
	game_to_string(game_string);
	printf("%s\n", game_string);
	return location_value;
}
