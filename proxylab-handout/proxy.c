/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS: (put your names here)
 *     Brian Scheper, bpsc222@g.uky.edu 
 *     Andrew Johnston, andrewjacobjohnston@gmail.com 
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */ 

#include <sys/socket.h>
#include "csapp.h"

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);

/* function that will read in disallowed words */
void readDisallowed(char** disallowed);

void proxy(int connfd);

//Global variables
FILE *logfile;

/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{
	int listenfd, connfd, port;
	socklen_t clientlen;
	struct sockaddr_in clientaddr;
	struct hostent *hp;
	char *clientip;

    /* Check arguments */
    if (argc != 2) {
	    fprintf(stderr,"Usage: %s <port number>\n", argv[0]);
	    exit(0);
    }

    char* disallowed[100];
    readDisallowed(disallowed);

    //Create the listening port
	port = atoi(argv[1]);
	listenfd = Open_listenfd(port);
	if (listenfd == -1)
		unix_error("Error listening to the port");

	//Open log file to append to
	logfile = fopen("proxy.log", "a");

	while (1) {
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

		/* Determine the domain name and IP address of the client */
		hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,sizeof(clientaddr.sin_addr.s_addr), AF_INET);
		clientip= inet_ntoa(clientaddr.sin_addr);
		printf("Client connected from %s (%s)\n", hp->h_name, clientip);
		proxy(connfd);
		Close(connfd);
	}
	exit(0);
}


/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
	hostname[0] = '\0';
	return -1;
    }
       
    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';
    
    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')   
	*port = atoi(hostend + 1);
    
    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
	    pathname[0] = '\0';
    }
    else {
	    pathbegin++;	
	    strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		      char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s", time_str, a, b, c, d, uri);
}


void readDisallowed(char** disallowed) {

    /* opening disallowed words file to read */
    FILE *fp;
    fp = fopen("DisallowedWords", "r");
    if (fp == NULL) {
	printf("Error opening DisallowedWords\n");
	return;
    }

    /* index for disallowed words array */
    int dis_index = 0;
    /* buffer for each line of disallowed words file */
    char line[100];
    char *eof;

    /* reading in a line from the file. Assuming a line isn't more than 100 characters.
	Put this line in the array. */
    while ((eof = fgets(line, 100, fp)) != NULL) {
	disallowed[dis_index] = strdup(line);
	dis_index++;
    }

    /* closing file descriptor for disallowed words */
    fclose(fp);
}

//Handles getting the client request and scanning it for disallowed words
void proxy(int connfd){
	size_t n;
	int serverfd;
	int port = 0;
	char buf[MAXLINE], uri[MAXLINE], hostname[MAXLINE], pathname[MAXLINE], header[MAXLINE];
	rio_t rio_client, rio_server;
	char *token;
	char method[20], version[20]; //Largest HTTP method (verb) is 16 chars, leave 20 for future.

	//Init the client socket
	Rio_readinitb(&rio_client, connfd);

	//Get the URI from the buffer
	n = Rio_readlineb(&rio_client, buf, MAXLINE);
	Rio_writen(connfd, buf, n);

	token = strtok(buf, " ");
	if (token != NULL){
		strcpy(method, token);

		token = strtok(NULL, " ");
		strcpy(uri, token);

		//Parse the URI, if we have an error close the connection
		pathname[0] = '/';
		if (parse_uri(token, hostname, pathname+1, &port) == -1){
			Close(connfd);
			return;
		}

		token = strtok(NULL, " ");
        strcpy(version, token);

		printf("method:%s\n", method);
		printf("uri:%s\n", uri);
		printf("version:%s\n", version);

		printf("hostname:%s\n", hostname);
		printf("pathname:%s\n", pathname);
		printf("port:%d\n", port);
	}
	while (token != NULL){ //Read to the end of the buffer
		token = strtok(NULL, " ");
	}

	//We now have our URI, connect to the server now
	printf("Open server connection...\n");
	if ((serverfd = Open_clientfd(hostname, port)) < 0){
		Close(connfd);
		return;
	}
	Rio_readinitb(&rio_server, serverfd);

	//Get the data from the server
	sprintf(header, "%s %s %s", method, pathname, version);
	Rio_writen(serverfd, header, strlen(header));

	printf("response..\n");
	while(((n = Rio_readlineb(&rio_client, buf, MAXLINE)) > 0) && buf[0] != '\r' && buf[0] != '\n'){
		printf("%s", buf);
		Rio_writen(serverfd, buf, n);
	}
	Rio_writen(serverfd, "\r\n", 2);
	
	printf("respond:%s\n", header);

	//Write respond back to the client
	printf("writing\n");
	while((n = Rio_readnb(&rio_server, buf, MAXLINE)) > 0){
		printf("%s\n", buf);
		Rio_writen(connfd, buf, n);
	}

	//Close our open connections
	Close(serverfd);
	Close(connfd);
}
