/* A simple server in the internet domain using TCP
   The port number is passed as an argument
   This version runs forever, forking off a separate
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>  /* signal name macros, and the kill() prototype */


void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno;
    int process_id;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);  // create socket
    if (sockfd < 0)
        error("ERROR opening socket");
    memset((char *) &serv_addr, 0, sizeof(serv_addr));   // reset memory

    // fill in address info
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    if (listen(sockfd, 10) < 0) {
        error("listen failed");
    }  // 5 simultaneous connection at most

    //accept connections
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

    if (newsockfd < 0)
        error("ERROR on accept");

    // SERVER request and response
    int n;
    char buffer[2048];
    char fn[1024];

    memset(buffer, 0, 2048);  // reset memory
    memset(fn, 0, 1024); // reset file name

    //read client's message
    n = read(newsockfd, buffer, 2047);
    if (n < 0) error("ERROR reading from socket");
    printf("Here is the message: %s\n", buffer);
    

    int file_descriptor; 
    file_descriptor = open("index.html", O_RDONLY);

    char* wrbuf[512];

    while (1) {
        int num_chars_read = read(file_descriptor, wrbuf, 512);

        //reply to client
        n = write(newsockfd, wrbuf, strlen(wrbuf));
        if (n < 0) error("ERROR writing to socket");
        if (num_chars_read == 0) {
            break;
        }
    }
    
    close(newsockfd);  // close connection
    close(sockfd);

    return 0;
}
