/* By Kuan Xiang Wen and Josh Camarena, Feb 2018
   CS118 Project 1
   
   Running this program starts a webserver. It is non-persistent – the server only handles a single HTTP request and then returns. 
   We have also provided “index.html” and “404.html” to handle the default and File DNE cases.
   The server awaits a HTTP GET request, and returns a single HTTP response. 
   To request a file from the server, type “(IP ADDRESS):(PORT NO.)/(FILENAME)” into an up-to-date Firefox browser. 
   It can return the following filetypes: htm/html, jpg/jpeg file, gif file. 
   If the filename is nonempty but has does not have any of the above filetypes, it returns a binary file that prompts a download. 
   If the field is empty, it looks to download “index.html”. 
   If the file is not found, it returns “404.html with a 404 error”.
   After it serves a request, the server closes all sockets and returns/shuts down. 
   To retrieve another file, the server binary should be run again.
*/
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

//The following are HTTP Response fields. They need to be global to work across parse_file_req() and response()
char* content_type;
char* content_response_code;
size_t content_length;

//Simple error handling function. Prints a message and exits when called.
void error(char *msg)
{
    perror(msg);
    exit(1);
}

//(Parse File Request) Parses the HTML GET request and prepares the file to send to client.
// Also sets string values for parts of the HTTP response header. 
int parse_file_req(char* buf){
    char filename[2048];
    bzero(filename,2048);
    //Isolate substring that is the filename
    char* fn_start = strstr(buf,"GET /") + 5; //fn_start will point at the first (nonslash) character of the file name
    char* fn_end = strstr(buf," HTTP/1.1\r\n");
    size_t fn_size = (fn_end - fn_start)/sizeof(char); //This is the size, in # of char, of the file name
    if(fn_size < 0 || fn_size > 2048){ //Error checking for invalid file name size
        printf("Invalid file name size: %d\n", (int) fn_size);
        exit(0);
    }
    //Special case: No/Empty file name requested => Default to index.html 
    if (fn_size == 0) {
        strcat(filename, "index.html");
        fn_size = 10;
    }
    else memcpy(filename,fn_start,fn_size);//Give the filename its own buffer
    
    //Replace "%20" strings with spaces
    char* marker;
    while((marker = strstr(filename,"%20")) != NULL){
        *marker = ' ';//Replace '%' with a space. We deal with the '2' '0' next line 
        strncpy(marker+1,marker+3,fn_size-((marker+3)-filename)/sizeof(char)); //Shift string two spaces back to overwrite the "20"
        fn_size = fn_size - 2; //"%20" -> ' ' has net length decreased by two
        memset(filename+fn_size*sizeof(char),0,2); //Erase last 2 characters to shorten string
    }

    //Extract file type
    marker = strrchr(filename,'.'); //Get filetype substring. Marker points to the final '.' char
    if (marker == NULL) {
        content_type = "application/octet-stream"; //No substring => binary file
    } else {
        marker = marker +1; //If marker exists next char points to the first character of the filetype substring
       //"Case Insensitive" filetype fix. Make all filetype char lowercase
        int i = 0; //Iterator
        for(;i<strlen(marker);i++){//Parse the substring. If ASCII value equals that of a capital letter, replace it with its 
           //lowercase equivalent. They are each spaced apart by exactly 32. Eg. 'G'+32='g'
            if(*(marker+i)>64 && *(marker+i) < 91) *(marker+i) = *(marker+i)+32;
        }
        //Actual comparison to append to content_type. Self explanatory
        if(strcmp(marker,"html")==0 || strcmp(marker,"htm")==0) content_type = "text/html";
        else if(strcmp(marker,"jpg")==0 || strcmp(marker,"jpeg")==0) content_type = "image/jpeg";
        else if(strcmp(marker,"gif")==0) content_type = "image/gif";
        else content_type = "application/octet-stream"; //Unrecognized filetype => Binary file
    }
    //Import file. We do the important after the filetype processing because all filetypes should be lowercase in the server
    int fd = open(filename, O_RDONLY);
    if(fd < 0){//If fail to import, import 404 file
        content_response_code = "404 Not Found";
        if((fd = open("404.html",O_RDONLY)) < 0){ //Try to open 404.html
            error("404 File is also not found\n");
        }
        content_type = "text/html";
    } else {//File found! No problem
        content_response_code = "200 OK";
    }

    //Extract file size using fstat()
    struct stat s; //Declare struct
    if (fstat(fd,&s) < 0){ //Attempt to use fstat()
         error("fstat() failed");
    }
    content_length = s.st_size; //The st_size field of the structure is what we want

    //Return file descriptor
    return fd;
}

//Handles server input/output. Calls parse_file_req()
void respond(){
    int n;
    char in_buffer[2048]; //Buffer for HTTP GET input
    char header[2048]; //Buffer for HTTP response header

    memset(in_buffer, 0, 2048);  // reset memory
    memset(header,0,2048); //reset header memory

    //read client's message
    n = read(newsockfd, in_buffer, 2047);
    if (n < 0) error("ERROR reading from socket");
    printf("%s\n", in_buffer);
    
    //Extract file name, get its file descriptor, and modify content_type, content_length and content_response_code
    int fd = parse_file_req(in_buffer);
   
    //Construct TCP header incrementally
    strcat(header,"HTTP/1.1 ");
    strcat(header,content_response_code);
    strcat(header,"\r\nContent-Type: ");
    strcat(header,content_type);
    strcat(header,"\r\nContent-Length: ");
    sprintf(header + strlen(header),"%d", (int) content_length);//sprintf here needed for integer
    strcat(header,"\r\nConnection: Keep-Alive\r\n\r\n");
   
    write(newsockfd, header, strlen(header));//Write header

    //Write file contents
    char* wrbuf = (char*) malloc(sizeof(char)*content_length); //Create buffer for HTTP response payload with exact size for the file
    read(fd, wrbuf, content_length); //Read file into the buffer
    write(newsockfd, wrbuf, content_length); //Write the buffer into the socket
    close(fd); //Close the file
}

//Sets up socket connection and starts server (based entirely off sample code). 
//Calls response() and closes socket and returns right after.
int main(int argc, char *argv[])
{//Socket connection as provided by sample code
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
