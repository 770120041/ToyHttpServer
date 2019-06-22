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



#define DEBUG

#define BUFFERSIZE 2048  // max number of bytes we can get at once 
#define FILENAME "output"

//./http_client http://www.example.com/
// ./http_client http://cs438.cs.illinois.edu/ 
// ./http_client   http://google.com/
// ./http_client http://bit.ly/2D13uvT123/
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

void send_message(int sockfd,struct addrinfo *p, char* msg){
	int numbytes = -1;
	if ((numbytes = sendto(sockfd,msg,strlen(msg),0, p->ai_addr, p->ai_addrlen)) == -1){
        perror("sendto");
		exit(1);
	}
   
    sprintf(debug_buffer, "client send:\n%s",msg);
    debug_print(debug_buffer);
	
}

char* get_header(int sockfd){
    char* header_buffer = (char*)malloc(sizeof(char) * BUFFERSIZE);
    int index = 0;
    while (recv(sockfd, &header_buffer[index], 1, 0) == 1) {
        header_buffer[index+1] = '\0';
        if(strstr(header_buffer,"\r\n\r\n") != NULL){
            debug_print("header detected");
            break;
        }
		index += 1;
	}
    return header_buffer;
}

int is_redircted(const char* header){
    char *sub_string;
    if((sub_string = strstr(header,"Location:")) != NULL){
        debug_print("redirecting detected");
        return sub_string-header;
    }
    return 0;
}

int recv_file(int sockfd, FILE* fp){

	int numbytes = -1;
    int received_bytes = 0;
	char recv_buffer[BUFFERSIZE];
	while ((numbytes = recv(sockfd, recv_buffer, BUFFERSIZE, 0)) > 0) {
		received_bytes += numbytes;
		fwrite(recv_buffer, sizeof(char), numbytes, fp);
    }

	if (numbytes == -1) {
		perror("recv");
		exit(1);
	}
    return received_bytes;
}

int main(int argc, char const *argv[])
{
    if(argc != 2){
        fprintf(stderr," Usage \"./http_client URL\"\n");
        exit(1);
    }
    
    char host_name[80], path_to_file[80];
    char port[80];
    char s[INET6_ADDRSTRLEN];
    int sockfd, numbytes;  
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int location_pos;

    //flag marks first time
    int first = 1;
    char* header;
    do{
        //free tmp url
        char* redirect_url = (char*)malloc(sizeof(char)*BUFFERSIZE);
        //first means not redirecting
        if(first){
            strcpy(redirect_url,argv[1]);
            first = 0;
            
        }
        else{
            //if it is redirection URL
            sscanf(header+location_pos+10,"%[^\n]", redirect_url);
            debug_print("redirecting URL is :");
            debug_print(redirect_url);
            int length = strlen(redirect_url);

            //replace \r with \n to make it using same parser
            redirect_url[length-1] = '\n';
            debug_print(redirect_url);
        }

        if(sscanf(redirect_url, "http://%79[^:]:%79[^/]%79[^\n]", host_name, port, path_to_file) == 3){
            // debug_print("3 detected");
            ;
        }else if(sscanf(redirect_url, "http://%79[^/]%79[^\n]", host_name, path_to_file) == 2){
            // debug_print("2 detected");
            strcpy(port,"80");
        }else{
            fprintf(stderr,"Cannot parse URL in redirect URL at given input!\n");
            exit(1);
        }

        free(redirect_url);
        
        
        debug_print("redirect parameters");
        sprintf(debug_buffer,"port = %s, filepath = %s, host_name = %s\n",port,path_to_file,host_name);
        debug_print(debug_buffer);
       
        //set up hints 
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        
        //get addr info 
        if ((rv = getaddrinfo(host_name, port, &hints, &servinfo)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            return 1;
        }


        // loop through all the results and connect to the first we can
        for(p = servinfo; p != NULL; p = p->ai_next) {
            if ((sockfd = socket(p->ai_family, p->ai_socktype,
                    p->ai_protocol)) == -1) {
                perror("client: socket");
                continue;
            }
          
            if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                close(sockfd);
                perror("client: connect");
                continue;
            }
            break;
        }

        if (p == NULL) {
            fprintf(stderr, "client: failed to connect\n");
            return 2;
        }

        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
                s, sizeof s);
        sprintf(debug_buffer,"client: connecting to %s\n", s);
        debug_print(debug_buffer);
        
        // all done with this structure
        freeaddrinfo(servinfo);

        char get_request_template[] = 
            "GET %s HTTP/1.0\r\n"
            "User-Agent: Wget/1.15 (linux-gnu)\r\n"
            "Host: %s:%s\r\n\r\n";

        char get_request[BUFFERSIZE];
        sprintf(get_request,
            get_request_template,
            path_to_file,host_name,port
        );

        send_message(sockfd,p,get_request);

        
        
        header = get_header(sockfd);

        debug_print("Receiving header:");
        debug_print(header);

    }while((location_pos = is_redircted(header)) != 0);

    //free dynamically allocated header
    free(header);

    

	// struct timeval tv;
	// tv.tv_sec = 1;
	// tv.tv_usec = 0;
	// if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,  sizeof tv))
	// {
	// 	perror("setsockopt");
	// 	return -1;
	// }

    FILE* fp;
    if((fp = fopen(FILENAME,"wb")) == NULL){
        perror("fopen");
        exit(1);
    }

    int received_bytes = recv_file(sockfd,fp);
    
    
	printf("file size: %d bytes\n", received_bytes);


    fclose(fp);
    close(sockfd);


    return 0;
}
