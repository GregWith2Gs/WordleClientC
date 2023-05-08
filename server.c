/* hw4.c */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>

extern int total_guesses;
extern int total_wins;
extern int total_losses;
extern char ** words;

int exitrq = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void signalhandler(int sig){
	if(sig == SIGUSR1) {
		printf("MAIN: SIGUSR1 rcvd; Wordle server shutting down...\n");
		exitrq = 1;
	}
	return;
}

struct data {
	int connection;
	char * word;
};

void * run_wordle(void * arg){
	setvbuf( stdout, NULL, _IONBF, 0 );
	struct data wdata = *((struct data*)(arg));
	unsigned short guesses_left = 6;
	char * word = (char *) wdata.word;
	int con = wdata.connection;
	while(guesses_left > 0){
		char * buffer = calloc(9, sizeof(char));
		printf("THREAD %lu: waiting for guess\n", (long unsigned) pthread_self());

		if (read(con, buffer, 6) <= 0){
			pthread_mutex_lock(&mutex);
			total_losses++;
			pthread_mutex_unlock(&mutex);
			guesses_left = 0;
			printf("client gave up; closing TCP connection...\n");
		} else {
			for(int i = 0; i < strlen(buffer); ++i) *(buffer + i) = tolower(*(buffer + i));
			printf("THREAD %lu: rcvd guess: %s\n", (long unsigned) pthread_self(), buffer);

			int valid = 0;
			for ( char ** ptr = words ; *ptr ; ptr++ ){
				if (strcmp(*ptr,buffer) == 0){
					valid = 1;
				}
			}
			if(valid){
				guesses_left--;
				pthread_mutex_lock(&mutex);
				total_guesses++;
				pthread_mutex_unlock(&mutex);
				int right = 0;

				if(strcmp(buffer, word) == 0) right = 1;

				char * guessret = calloc(6, sizeof(char));
				for (int x = 0; x < 5; ++x){
					*(guessret + x) = '-';
					for(int y = 0; y < 5; y++){
						if(*(buffer + x) == *(word + y) && !isalpha(*(guessret + x))) *(guessret + x) = *(buffer + x);
					}
					if(*(buffer + x) == *(word + x)) *(guessret + x) = toupper(*(buffer + x));
				}
				*(guessret + 5) = '\0'; 
				printf("THREAD %lu: sending reply: %s (%hu guess%s left)\n", (long unsigned) pthread_self(),
					guessret, guesses_left, guesses_left == 1 ? "" : "es");
				*(buffer) = 'Y';
				uint16_t gr = htons(guesses_left);
				memcpy(buffer + 1, &gr, 2);
				strcpy((buffer + 3), guessret);
				write(con, buffer, 9);
				free(guessret);

				if (right){
					pthread_mutex_lock(&mutex);
					total_wins++;
					pthread_mutex_unlock(&mutex);
					guesses_left = 0;
				} else if (guesses_left == 0){
					pthread_mutex_lock(&mutex);
					total_losses++;
					pthread_mutex_unlock(&mutex);
				}
			} else {
				*(buffer) = 'N';
				uint16_t gr = htons(guesses_left);
				memcpy(buffer + 1, &gr, 2);
				strcpy((buffer + 3), "?????");
				printf("THREAD %lu: invalid guess; sending reply: ????? (%hu guess%s left)\n",
					(long unsigned) pthread_self(), guesses_left, guesses_left == 1 ? "" : "es");
				write(con, buffer, 9);
			}
		}
		free(buffer);
	}
	for (int x = 0; x < 5; ++x) *(word + x) = toupper(*(word + x));
	printf("THREAD %lu: game over; word was %s!\n", (long unsigned) pthread_self(), word);
	free(arg);
	free(word);
	close(con);
	return(NULL);
}




int wordle_server(int argc, char ** argv){
	signal(SIGINT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
	struct sigaction handler;
	handler.sa_handler = signalhandler;
	handler.sa_flags = 0;
	sigemptyset( &handler.sa_mask );
	sigaction( SIGUSR1, &handler, NULL );

	if(argc != 5){
		fprintf(stderr, "ERROR: Invalid argument(s)\nUSAGE: hw4.out <listener-port> <seed> <word-filename> <num-words>\n");
     	return EXIT_FAILURE;
	}
	int portnum = (unsigned short) atoi(*(argv + 1));
	if (portnum <= 0){ // Gets size of cache - must be greater than 0. atoi returns 0 if bad input
    	fprintf(stderr, "ERROR: Invalid argument(s)\nUSAGE: hw4.out <listener-port> <seed> <word-filename> <num-words>\n");
    	return EXIT_FAILURE;
	}
	int randseed = atoi(*(argv + 2));
	srand(randseed);


	int size = atoi(*(argv + 4));
	words = (char **) realloc(words, size * sizeof(char *) + 1);

	FILE * pfile;
	pfile = fopen(*(argv + 3), "r");
	if (pfile == NULL){
		fprintf(stderr, "ERROR: Invalid argument(s)\nUSAGE: hw4.out <listener-port> <seed> <word-filename> <num-words>\n");
		return EXIT_FAILURE;
	}
	printf("MAIN: opened %s (%d words)\n", *(argv + 3), size);

	size_t len = 0;
	for(int x = 0; x < size; ++x){
		*(words + x) = calloc(6, sizeof(char));
		int read = getline((words + x), &len, pfile);
		if (read != 0){
			*(*(words + x) + 5) = '\0';
		} else {
			fprintf(stderr, "ERROR: Bad file, or incorrect size count\n");
			fclose(pfile);
     		return EXIT_FAILURE;
		}
	}
	fclose(pfile);
	*(words + size) = NULL;

	int listener = socket(AF_INET, SOCK_STREAM, 0);
	if(listener == -1){
		fprintf(stderr, "ERROR: Socket failure\n");
	}

	
	struct sockaddr_in tcp_server;
	tcp_server.sin_family = AF_INET;
	tcp_server.sin_port = htons(portnum);
	tcp_server.sin_addr.s_addr = INADDR_ANY;

	if (bind(listener, (struct sockaddr *) &tcp_server, sizeof(tcp_server)) == -1){
		fprintf(stderr, "ERROR: Unable to bind\n");
		return EXIT_FAILURE;
	}

	if (listen(listener, 5) == -1){
		fprintf(stderr, "ERROR: Listen failed\n");
	}
	printf("MAIN: Wordle server listening on port {%d}\n", portnum);

	char ** used = calloc(1, sizeof(char *));
	int played = 0;
	while(!exitrq){
		pthread_t child;

		struct sockaddr_in client;
		socklen_t fromlen = sizeof(client);

		int con = accept(listener, (struct sockaddr *) &client, &fromlen);
		if(con != -1) {
			printf("MAIN: rcvd incoming connection request\n");
			struct data * wordle_data = calloc(1, sizeof(struct data));
			wordle_data->connection = con;
			int n = rand() % size;
			wordle_data->word = calloc(6, sizeof(char));
			strcpy(wordle_data->word, *(words + n));
			pthread_create(&child, NULL, run_wordle, (void *) wordle_data);
			pthread_detach(child);

			*(used + played) = calloc(6, sizeof(char));
			strcpy(*(used + played), *(words + n));
			++played;
			used = (char **) realloc(used, played * sizeof(char *));

		}
	}

	free(used);

	return EXIT_SUCCESS;
}