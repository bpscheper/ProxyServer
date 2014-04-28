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

#include "csapp.h"

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void readDisallowed(char** disallowed);
void proxy(int connfd);
int isDisallowed(char* disallowed, char* line, int length);
void newContent(char* content);

//Global variables
FILE *logfile;


/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv){
	/* Check arguments */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
		exit(0);
	}

	//Create the array of disallowed words
    char* disallowed[100] = { '\0' };
    readDisallowed(disallowed);
	
	//Create the listening port
	int port = atoi(argv[1]), connfd, listenfd;
	socklen_t clientlen = sizeof(struct sockaddr_in);
	struct sockaddr_in clientaddr;
	struct hostent *hp;
	char *clientip;
	listenfd = Open_listenfd(port);

	//Starting listening for client connections
	while(1) {
		connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
		hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,sizeof(clientaddr.sin_addr.s_addr), AF_INET);
		clientip= inet_ntoa(clientaddr.sin_addr);

		printf("Client connected from %s (%s)\n", hp->h_name, clientip);
		proxy(connfd);
		//Close(connfd);
		printf("Connection closed\n");
	}

	char* line = "Stupid words";
	char* content;

	int index = 0;
	while (disallowed[index] != 0) {
		int count = 0;
		while (disallowed[index][count] != '\0') {
			count++;
		}
		int dis = isDisallowed(disallowed[index], line, count);
		if (dis == 1)
			newContent(content);
		else
			printf("That is fine");
		fflush(stdout);
		index++;
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


void readDisallowed(char** disallowed){
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
void proxy(int connfd) {
	size_t n;
	int clientfd, port, clen, cread, next;
	char buf[MAXLINE], hostname[MAXLINE], pathname[MAXLINE];
	rio_t rio_client, rio_server;
	char tempbuffer[MAXLINE];
	
	//Set the memory here
	memset(tempbuffer, 0, MAXLINE);

	//Get the URI from the client request
	Rio_readinitb(&rio_client, connfd);
	while((n = Rio_readlineb(&rio_client, buf, MAXLINE)) != 0) {
		if(parse_uri(strstr(buf, " ")+1, hostname, pathname, &port)) {
			printf("Could not get URI\n");
			return;
		}
		clientfd = Open_clientfd(hostname, port);
		Rio_readinitb(&rio_server, clientfd);
		Rio_writen(clientfd, buf, n);

		//Get the request from the client
		while((n = Rio_readlineb(&rio_client, buf, MAXLINE)) != 0) {
			Rio_writen(clientfd, buf, n);

			//Check to see that we have hit the line break in the header yet
			if(!strcmp(buf, "\r\n"))
				break;
		}

		//Get the response from the server
		clen = 0;
		next = 0;
		while((n = Rio_readlineb(&rio_server, buf, MAXLINE)) != 0) {
			Rio_writen(connfd, buf, n);

			//try to determine the header length
			if(strstr(buf, "Content-Length") != NULL) {
				clen = atoi(strstr(buf, " ")+1);
			}

			//Read the rest of the response
			if(!strcmp(buf, "\r\n")) {
				if((clen < 0) && !next){
					next = 1;
					continue;
				}
				else if((clen < 0) && next){
					break;
				}
				
				//Determine the length of the response
				cread = 0;
				while(cread < clen) {
					if ((clen - cread) < MAXLINE){
						n = Rio_readnb(&rio_server, buf, clen-cread);
					}
					else{
						n = Rio_readnb(&rio_server, buf, MAXLINE);
					}
					
					if(n == 0) {
						break;
					}
					cread += n;

					//Copy the string into the temp buffer
					strcat(tempbuffer, buf);
				}
				break;
			}
		}
		break;
	}

	//Check the tempbuffer for disallowed characters
	Rio_writen(connfd, buf, n);

	int test = isDisallowed(disallowed, buf, MAXLINE);
	printf("test:%d", test);
}

//Checks to see if the string contains a disallowed character
int isDisallowed(char* disallowed, char* line, int length) {
	int i = 0;
	int index = 0;

	while ((i < 100) && (line[i] != '\0')) {
		if (disallowed[index] == line[i]) {
			index++;
			if (index == (length-1)) {
				return 1;
			}
		} else {
			index = 0;
		}
		i++;
	}
	return 0;
}
