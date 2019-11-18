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
#include<sys/sem.h>
#include<netinet/in.h>
#include<netdb.h>
#include<unistd.h>

#define SHMKEY ((key_t) 7890)
#define SEMKEY ((key_t) 400L)
#define NSEMS 1
#define PORTNUM  7777 /* the port number the server will listen to*/
#define DEFAULT_PROTOCOL 0  /*constant for default protocol*/

void doprocessing(int sock, int waiting);
void game_to_string(char *game_string);
void generate_game_matrix();
int select_location(char location, char *name);
void exit_client(int was_ready);

enum Stages {
	AWAITING_CLIENTS, // Waiting for at least 2 clients to be connected and ready
	PLAYING, // The game has started and any new client connections must wait for it to be over
};

char name[20];
int points;

typedef struct {
	enum Stages stage;
	int num_clients;
	int num_ready;
	int game_matrix[16];
	int locations_left;
	char winner[20];
	int winner_pts;
} shared_mem;
shared_mem *client_data;

union {
	int val;
	struct semid_ds *buf;
	ushort *array;
} semctl_arg;

static struct sembuf Wait = {0, -1, 0};
static struct sembuf Signal = {0, 1, 0};

int semid, status, semnum, semval;

int main(int argc, char *argv[])  {
	int sockfd, newsockfd, portno, clilen;
   	char buffer[256];
   	struct sockaddr_in serv_addr, cli_addr;
   	int status, pid, pid2;
	
	// Shared memory
	key_t key = 123;   
	int shmid;
	char *shmadd;
	shmadd = (char *) 0;

	if((shmid = shmget (SHMKEY, sizeof(int), IPC_CREAT | 0666)) < 0) {
		perror("shmget");
		exit(1);
	}

	if((client_data = (shared_mem *) shmat(shmid, shmadd, 0)) == (shared_mem *) -1) {
		perror("shmat");
		exit(0);
	}

	semid = semget(SEMKEY, NSEMS, IPC_CREAT | 0666);
	if(semid < 0) {
		printf("Error creating semaphore.");
		exit(1);
	}

	semnum = 0;
	semctl_arg.val = 1;
	status = semctl(semid, semnum, SETVAL, semctl_arg);
	semval = semctl(semid, semnum, GETVAL, semctl_arg);

	// initialize
	client_data->stage = AWAITING_CLIENTS;
	client_data->num_clients = 0;
	client_data->num_ready = 0;
	client_data->locations_left = 0;
	client_data->winner_pts = -100;
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

	pid2 = fork(); // the game state process
	if(pid2 == 0) { 
		while(1) {
			if(client_data->stage == AWAITING_CLIENTS && client_data->num_clients >= 2 && client_data->num_ready == client_data->num_clients) {
				status = semop(semid, &Wait, 1);
				client_data->winner_pts = -100;
				generate_game_matrix();
				char game_string[32] = "";
				game_to_string(game_string);
				printf("%s\n", game_string);
				client_data->stage = PLAYING;
				status = semop(semid, &Signal, 1);
			} else if(client_data->stage == PLAYING && client_data->locations_left == 0) {
				printf("All game locations have been selected. %s is the winner with %d points!\n", client_data->winner, client_data->winner_pts);
				status = semop(semid, &Wait, 1);
				client_data->num_ready = 0;
				client_data->stage = AWAITING_CLIENTS;
				status = semop(semid, &Signal, 1);
			}
		}
		if((shmctl(shmid, IPC_RMID, (struct shmid_ds *) 0)) == -1) {
			perror("shmctl");
			exit(-1);
		}
		semctl_arg.val = 0;
		semnum = 0;
		status = semctl(semid, semnum, IPC_RMID, semctl_arg);
		if(status < 0) {
			printf("Error releasing semaphore.");
			exit(1);
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
      
      		/* Create child process for client */
      		pid = fork();
		
      		if (pid < 0) {
         		perror("ERROR on fork");
         		exit(1);
      		}

		if(pid > 0) {
			status = semop(semid, &Wait, 1);
			client_data->num_clients++;
			status = semop(semid, &Signal, 1);
			printf("Client connected. Number of connected clients: %d.\n", client_data->num_clients);      
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
   	}
}


void doprocessing (int sock, int waiting) {
	int replay = 0;
	write(sock, (const char *) &(client_data->stage), 4); // tell client which stage we're currently on
	while(1) {	
		int socket_status;
   		char buffer[256];
		
		if(client_data->stage == AWAITING_CLIENTS) {
			if(waiting == 1) { // put any clients that are waiting back on track
				waiting = 0;
				socket_status = write(sock, (const char *) &(client_data->stage), 4);
			}
			bzero(buffer, 256);
			if(replay == 0) // prompt based on if the user has just finished a game
				strcat(buffer, "Please enter your name to begin playing: ");
			else
				strcat(buffer, "Would you like to play again? (y/n): ");
			socket_status = write(sock, buffer, strlen(buffer));
			bzero(buffer, 256);
			socket_status = read(sock, buffer, 256);
			if(replay == 0) {
				int i = 0;
				while(i < 20) { // read first 20 characters, or until the first white space/new line
					if(buffer[i] == ' ' || buffer[i] == '\n')
						break;
					name[i] = buffer[i];
					i++;
				}
			} else if(replay == 1 && (buffer[0] == 'n' || buffer[0] == 'N')) {
				status = semop(semid, &Wait, 1);
				exit_client(0);
				status = semop(semid, &Signal, 1);
				break;
			}
			status = semop(semid, &Wait, 1);
			client_data->num_ready++;
			status = semop(semid, &Signal, 1);
			replay = 0;
			while(client_data->stage == AWAITING_CLIENTS);
			bzero(buffer, 256);
			game_to_string(buffer);
			socket_status = write(sock, buffer, 256);
		} else if(client_data->stage == PLAYING) {
			if(waiting == 1) // ignore any clients that are waiting for now
				continue;
			replay = 1;
			while(client_data->locations_left > 0) {
				char location;
				socket_status = read(sock, &location, 1);
				status = semop(semid, &Wait, 1);
				int pts = select_location(location, name);
				status = semop(semid, &Signal, 1);
				bzero(buffer, 256);
				if(pts > -10)
					snprintf(buffer, sizeof(buffer), "You receive %d point(s) for selecting location %c, and now have a total of %d points.\n", pts, location, points);
				else if(pts == -10)
					snprintf(buffer, sizeof(buffer), "Location '%c' has already been claimed.\n", location);
				else if(pts == -20)
					snprintf(buffer, sizeof(buffer), "Invalid location '%c'.\n", location);
				else if(pts == -30)
					snprintf(buffer, sizeof(buffer), "This game has finished. Your score was: %d.\n", points);
				game_to_string(buffer);
				if(client_data->locations_left == 0) {
					if(strcmp(name, client_data->winner) == 0)
						snprintf(buffer, sizeof(buffer), "Congratulations, you won! Total score: %d points.\n", client_data->winner_pts);
					else
						snprintf(buffer, sizeof(buffer), "Looks like you were beaten by %s, with a total score of %d points. Better luck next time!\n", client_data->winner, client_data->winner_pts);
					points = 0;
				}
				socket_status = write(sock, buffer, strlen(buffer));
				socket_status = write(sock, (const char *) &(client_data->stage), 4); // tell the client whether or not the game is finished
			}
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
	if(client_data->stage != PLAYING)
		return -30;

	if(location < 97 || location > 112)
		return -20;

	int index = location - 97;
	if(client_data->game_matrix[index] == -50)
		return -10;

	int location_value = client_data->game_matrix[index];
	points += location_value;
	if(points > client_data->winner_pts) {
		client_data->winner_pts = points;
		strcpy(client_data->winner, name);
	}
	printf("%s selected location '%c' for %d points, and now has a total of %d points.\n\n", name, location, location_value, points);
	client_data->game_matrix[index] = -50;
	client_data->locations_left--;
	char game_string[50] = "";
	game_to_string(game_string);
	printf("%s\n", game_string);
	return location_value;
}

void exit_client(int was_ready) {
	client_data->num_clients--;
	if(was_ready == 1)
		client_data->num_ready--;
	printf("A client has exited. Number of connected clients: %d.\n\n", client_data->num_clients);
}
