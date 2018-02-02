#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>  /* signal name macros, and the kill() prototype */

int sockfd, newsockfd, portno;

char* content_type;
char* content_response_code;
size_t content_length;

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int parse_file_req(char* buf){
    char filename[2048];
    bzero(filename,2048);
    //Isolate substring that is the filename
    char* fn_start = strstr(buf,"GET /") + 5;
    char* fn_end = strstr(buf," HTTP/1.1\r\n");
    size_t fn_size = (fn_end - fn_start)/sizeof(char);
    if(fn_size < 0 || fn_size > 2048){
        printf("Invalid file name size: %d\n",fn_size);
        exit(0);
    }
    //Special case: No/Default file requested index.html 
    if (fn_size == 0) {
        strcat(filename, "index.html");
        fn_size = 10;
    }
    else memcpy(filename,fn_start,fn_size);
    
    //Replace "%20" strings with spaces
    char* marker;
    while((marker = strstr(filename,"%20")) != NULL){
        *marker = ' ';
        strncpy(marker+1,marker+3,fn_size-((marker+3)-filename)/sizeof(char));
        fn_size = fn_size - 2;
        memset(filename+fn_size*sizeof(char),0,2);
    }
    //printf("File name: %s\n",filename);
    //Extract file type
    marker = strrchr(filename,'.');
    if(marker == NULL || strcmp(marker+1,"html")==0) content_type = "text/html";
    else if(strcmp(marker+1,"jpg")==0) content_type = "image/jpeg";
    else if(strcmp(marker+1,"gif")==0) content_type = "image/gif";
    else content_type = "application/octet-stream";
    //Import file
    int fd = open(filename, O_RDONLY);
    if(fd < 0){
        content_response_code = "404 Not Found";
        if((fd = open("404.html",O_RDONLY)) < 0){
            error("404 File is also not found\n");
        }
        content_type = "text/html";
    } else {
        content_response_code = "200 OK";
    }
    struct stat s;
    if (fstat(fd,&s) < 0){
         error("fstat() failed");
    }
    content_length = s.st_size;
    return fd;
}

//Responds to one HTTP GET request
void respond(){
    int n;
    char in_buffer[2048];
    char fn[1024];
    char header[2048];
    char wrbuf[512];
    bzero(wrbuf, sizeof(wrbuf));

    memset(in_buffer, 0, 2048);  // reset memory
    memset(fn, 0, 1024); // reset file name
    memset(header,0,2048); //reset header memory

    //read client's message
    n = read(newsockfd, in_buffer, 2047);
    if (n < 0) error("ERROR reading from socket");
    printf("Here is the message: %s\n", in_buffer);
    
    //Extract file name, get its file descriptor, and modify content_type
    int fd = parse_file_req(in_buffer);

    strcat(header,"HTTP/1.1 ");
    strcat(header,content_response_code);
    strcat(header,"\r\nContent-Type: ");
    strcat(header,content_type);
    strcat(header,"\r\nContent-Length: ");
    sprintf(header + strlen(header),"%d",content_length);
    strcat(header,"\r\n\r\n");
    write(newsockfd, header, strlen(header));

    while (1) {
        int num_chars_read = read(fd, wrbuf, 512);

        //reply to client
        n = write(newsockfd, wrbuf, strlen(wrbuf));
	bzero(wrbuf, sizeof(wrbuf));
        if (n < 0) error("ERROR writing to socket");
        if (num_chars_read == 0) {
            break;
        }
    }
}

int main(int argc, char *argv[])
{
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
    respond();
    
    close(newsockfd);  // close connection
    close(sockfd);

    return 0;
}
