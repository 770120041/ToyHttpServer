#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>

// #define DEBUG

#define BUFFERSIZE 2048  // max number of bytes we can get at once 

#define HTTP_200_HEADER "HTTP/1.0 200 OK\r\n\r\n"
#define HTTP_400_HEADER "HTTP/1.0 400 Bad Request\r\n\r\n"
#define HTTP_404_HEADER "HTTP/1.0 404 Not Found\r\n\r\n"

#define BACKLOG 64

char debug_buffer[BUFFERSIZE];

void debug_print(const char *msg){
    #ifdef DEBUG
        printf("%s\n",msg);
    #endif
}

void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


//becasue fork will creat threads, so we need to reap all dead threads as the book indicated
void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

void send_message(int sockfd, char* msg){
	int numbytes = -1;
	if ((numbytes = send(sockfd,msg,strlen(msg),0)) == -1){
        perror("send");
		exit(1);
	}

    
}

char* receive_message(int sockfd ){

	char* recv_buffer = malloc(sizeof(char)*BUFFERSIZE);
    int numbytes = -1;

	if ((numbytes = recv(sockfd, recv_buffer, BUFFERSIZE-1, 0)) == -1) {
	    perror("recv");
	    exit(1);
	}

	recv_buffer[numbytes] = '\0';

	if (numbytes == -1) {
		perror("recv");
        free(recv_buffer);
		exit(1);
	}
    return recv_buffer;
}

void send_response(int sockfd, char* file_name){
    FILE* fp = fopen(file_name,"rb");
    if(fp == NULL){
        char http_404_response[BUFFERSIZE];
        sprintf(http_404_response,"%s<!doctype html><html>file requested named: \"%s\" not found!</html>",
        HTTP_404_HEADER,file_name);
        printf("Send to clinet header:%s\n\n",http_404_response);
        send_message(sockfd,http_404_response);
        return ;
    }
    else{
        send_message(sockfd,HTTP_200_HEADER);
        printf("Send to clinet header:%s\n",HTTP_200_HEADER);

        
        debug_print("read begin");
        
        char file_read_buf[BUFFERSIZE];
        file_read_buf[BUFFERSIZE-1] = '\0';
        size_t bytes_read;
        while(!feof(fp)) {
            bytes_read = fread(file_read_buf, 1, BUFFERSIZE, fp);
            // file_read_buf[bytes_read] = '\0';
            // send_message(sockfd,file_read_buf);
            if (send(sockfd, file_read_buf, bytes_read, 0) == -1) {
                perror("send file");
            }
        }
        fclose(fp);

    }
}

int main(int argc, char const *argv[])
{
    if(argc != 2){
        fprintf(stderr,"Usage: ./http_server port\n");
        exit(1);
    }
    const char *port = argv[1] ;

    int sockfd, numbytes;  
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    struct sockaddr_storage their_addr; //clients' address lens
    int new_fd; //client's socket
    socklen_t their_size;
    struct sigaction sa;

    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // receive sockets
    

	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

    for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}
        int socket_reuse_literal = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &socket_reuse_literal,
			sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    if (listen(sockfd, BACKLOG) == -1) {
		perror("server:listen");
		exit(1);
	}


    freeaddrinfo(servinfo);

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server started at port %s!\n",port);

    while(1){
        their_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &their_size);
        if (new_fd == -1) {
			perror("accept");
			continue;//not terminates 
		}
        inet_ntop(their_addr.ss_family,
		    get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("server: Client address: %s\n", s);

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the parent
            
            char *header = receive_message(new_fd);

            debug_print("Server received header:");
            debug_print(header);\
            char* get_pos;
            if((get_pos = strstr(header,"GET")) == NULL){
                send_message(new_fd,HTTP_400_HEADER);
                printf("Send to clinet header:%s\n",HTTP_400_HEADER);

            }
            else{
                char get_line[BUFFERSIZE];
                char path_to_file[BUFFERSIZE];
                sscanf(get_pos,"%[^\n]",get_line); 
                sscanf(get_line+4,"%*c%s",path_to_file);
                send_response(new_fd,path_to_file);
                printf("send completed\n");
            }

            free(header);
            close(new_fd);
            exit(0);//exit child process
        }

        close(new_fd);  // parent doesn't need this

    }


    
    return 0;
}
