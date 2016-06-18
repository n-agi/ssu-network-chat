#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#define BUFFER_SUCCESS 0
#define STDIN_FILENO 0
int b_in;
int b_left;
char *token;
int find_newline(char *message, int in){
	int i = 0;
	for(i = 0 ; i < in ; i++){
		if(*(message+i) == '\n')
			return i;
	}
	return -1;
}
int buffer(char *message, int fd){
	int i = 0;
	size_t bytes = read(fd, token, 256 - b_in);
        int flag = -1;
	if(bytes <= 0 && fd != STDIN_FILENO){
		printf("Server is disconnected.\n");
		close(fd);
		return flag;
	}
	int newline_idx=0;
	b_in += bytes;
	newline_idx = find_newline(message,b_in);
	if(newline_idx >= 0){
		message[newline_idx] = 0x00;
		memmove(message, message+newline_idx+1, b_in - (newline_idx + 1));
		b_in -= (newline_idx+1);
		flag = BUFFER_SUCCESS;
	}
	b_left = sizeof(message) - b_in;
	token = message+b_in;
	return flag;
}

int main(int argc, char *argv[]){
	struct sockaddr_in serv_addr;
	char *addr;
	char *id;
	int port=0;
	int sockfd=0;
	int i;
	char rbuf[4096];
	char buf[4096];
	bool close_sent = false;
	fd_set all_set,tmp_set;
	if(argc == 4){
		addr = argv[2];
		port = atoi(argv[3]);
	}
	else if(argc == 2){
		id = argv[1];
		addr = "127.0.0.1";
		port = 8888;
	}
	else{
		addr = "127.0.0.1";
		port = 8888;
		id = "DefaultUser";
	}
	printf("[DEBUG] %s, Request to %s:%d\n",id, addr,port);
	if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1){
		perror("Socket error");
		exit(1);
	}
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = inet_addr(addr);	
	if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1){
		perror("Connect error");
		exit(1);
	}
	if(write(sockfd,id,strlen(id)+1) <= 0){
		perror("write id error");
		exit(1);
	}
	FD_ZERO(&all_set);
	FD_SET(STDIN_FILENO, &all_set);
	FD_SET(sockfd, &all_set);
	while(true){
		tmp_set = all_set;
		bzero(rbuf,sizeof(rbuf));
		bzero(buf,sizeof(buf));
		if(select(sockfd+1,&tmp_set,NULL,NULL,NULL) == -1){
			perror("select failed");
			exit(1);
		}
		if(FD_ISSET(STDIN_FILENO, &tmp_set)){
			token = rbuf;
			if(buffer(rbuf,STDIN_FILENO) == BUFFER_SUCCESS){
				if(write(sockfd,rbuf,strlen(rbuf)+1) == -1){
					perror("write failed");
					close(sockfd);
				}
				if(!strncmp("@quit", rbuf, 5)){
					if(shutdown(sockfd, SHUT_WR) == -1){
						perror("shutdown failed");
					}
					close_sent = true;
				}
			}
		}
		else if(FD_ISSET(sockfd, &tmp_set)){
			token = buf;
			while(buffer(buf, sockfd) == -1);
			if(close_sent){
				if(!strncmp(buf,"Goodbye!",8)){
					printf("Half-close finish\n");
					close(sockfd);
					break;
				}
			}
			printf("%s\n", buf);
			buf[0] = 0x00;
		}
	}
	exit(0);
}
