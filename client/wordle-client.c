/* hw4-client.c */

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>

int main(int argc, char ** argv) {
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) { perror("socket() failed"); exit(EXIT_FAILURE); }
    struct hostent * hp = gethostbyname( "localhost" ); // change depending on server structure

    if ( hp == NULL ){
        fprintf(stderr, "ERROR: gethostbyname() failed\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in tcp_server;
    tcp_server.sin_family = AF_INET;  // IPv4
    memcpy((void *)&tcp_server.sin_addr, (void *)hp->h_addr, hp->h_length);
    unsigned short server_port = atoi(*(argv + 1));
    tcp_server.sin_port = htons(server_port);

    printf("CLIENT: TCP server address is %s\n", inet_ntoa(tcp_server.sin_addr));

    printf("CLIENT: connecting to server...\n");

    if ( connect(sd, (struct sockaddr *)&tcp_server, sizeof(tcp_server)) == -1) {
        perror("connect() failed");
        return EXIT_FAILURE;
    }

    while (1) {
        char * buffer = calloc(9, sizeof(char));
        if (fgets(buffer, 9, stdin) == NULL) {
            free(buffer);
            break;
        }
        if (strlen(buffer) != 6) { printf("CLIENT: invalid -- try again\n"); continue; }
        *(buffer + 5) ='\0';   

        printf( "CLIENT: Sending to server: %s\n", buffer );
        int n = write( sd, buffer, strlen( buffer ) );
        if ( n == -1 ) { perror( "write() failed" ); return EXIT_FAILURE; }

        n = read(sd, buffer, 9);

        int correct = 5;

        if ( n == -1 ){
            perror("read() failed");
            free(buffer);
            return EXIT_FAILURE;
        } else if (n == 0) {
            printf("CLIENT: rcvd no data; TCP server socket was closed\n");
            free(buffer);
            break;
        } else {
            switch ( *buffer ){
                case 'N': printf("CLIENT: invalid guess -- try again\n"); break;
                case 'Y': 
                    printf("CLIENT: response: %s\n", buffer + 3);
                    for(int x = 3; x < 8; ++x){
                        if(*(buffer + x) != '-') correct--;
                    }
                    break;
            }
        
        short guesses = ntohs( *(short *)(buffer + 1) );
        if(correct == 0){
            printf("CLIENT: well done! you solved with %d guess%s remaining\n", guesses, guesses == 1 ? "" : "es");
            break;
        }
        if (guesses == 0) {
            printf("CLIENT: better luck next time!\n");
            break;
        }
        printf(" -- %d guess%s remaining\n", guesses, guesses == 1 ? "" : "es");
        }
    }

    printf("CLIENT: disconnecting...\n");

    close( sd );

    return EXIT_SUCCESS;
}
