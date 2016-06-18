#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#ifdef FD_SETSIZE
#define FD_MAX FD_SETSIZE
#else
#define FD_MAX 512
#endif
#define BUFFER_FINISH 1
#define BUFFER_REMAINS 0
struct s_client{
	char buffer[8096];
	int buf_idx;
	char username[64];
} *connection_list[FD_MAX];
int serv_sock;
struct sockaddr_in serv_addr;
fd_set prev;
void ConnectionClose(int fd){
	//logout message
	char logout[4096];
	int j = 0;
	snprintf(logout, 4096,"User %s disconnected.\n",connection_list[fd]->username);
	FD_CLR(fd, &prev);
	if(connection_list[fd]->username[0] > 0x00){
		for(j = 3 ; j < FD_MAX ; j++){
			if(fd != j && serv_sock != j && FD_ISSET(j, &prev)){
				if(write(j, logout, strlen(logout)) < 0){
					ConnectionClose(j);	
				}
			}
		}
	}
	if(connection_list[fd]){
		free(connection_list[fd]);
		connection_list[fd] = (struct s_client *)0x00000000;
	}
	printf("[DEBUG] %s",logout);
	close(fd);
}
void whisper(int sockfd){
	char msg_buf[4096];
	char *id = strchr(connection_list[sockfd]->buffer, ' ')+1;
	char *msg;
	bool found=false;
	int j =0;
	if(id == NULL){
		snprintf(msg_buf,4096,"Argument is wrong!\n");
		if(write(sockfd, msg_buf, strlen(msg_buf)) <= 0){
			ConnectionClose(sockfd);
		}
		return;
	}
	else{
		char *end = strchr(id, ' ');
		if(end == NULL){
			snprintf(msg_buf, 4096, "Argument is wrong!\n");
			if(write(sockfd, msg_buf, strlen(msg_buf)) <= 0){
				ConnectionClose(sockfd);
			}
			return;
		}
		else{
			*end = 0x00;
			msg = end+1;
			for(j = 3 ; j < FD_MAX ; j++){
				if(serv_sock != j && FD_ISSET(j, &prev)){
					if(!strncmp(connection_list[j]->username, id, (end-id)) && (strlen(connection_list[j]->username) == strlen(id))){
						if(sockfd == j){
							snprintf(msg_buf,1024,"You are whispering to yourself!\n");
							if(write(sockfd,msg_buf,strlen(msg_buf)) <= 0){
								ConnectionClose(sockfd);
							}
							found=true;
							continue;
						}
						found = true;
						snprintf(msg_buf,1024,"%s->%s: %s\n", connection_list[sockfd]->username, connection_list[j]->username, msg);
						if(write(j,msg_buf,strlen(msg_buf)) <= 0){
							ConnectionClose(j);
						}
						if(write(sockfd,msg_buf,strlen(msg_buf)) <= 0){
							ConnectionClose(sockfd);
						}
					}
				}
			}
			if(!found){
				if(write(sockfd,"User not found!\n", strlen("User not found!\n")) <= 0){
					ConnectionClose(sockfd);
				}
			}
		}
	}
}
void echoList(int sockfd){
	char buf[4096];
	int i =0;
	snprintf(buf,4096,"%s\n", "--User List--");
	if(write(sockfd, buf,strlen(buf)) <= 0){
		ConnectionClose(i);          

	}
	for(i = 3 ; i < FD_MAX ; i++){       
		if(!connection_list[i])      
			continue;
		if(serv_sock != i && FD_ISSET(i, &prev)){
			snprintf(buf,4096,"%s\n",connection_list[i]->username);
			if(write(sockfd,buf,strlen(buf)) <= 0){
				ConnectionClose(sockfd);
			}
		}
	}
	snprintf(buf,4096-strlen(buf),"%s\n","-------------");
	if(write(sockfd, buf, strlen(buf)) <= 0){
		ConnectionClose(sockfd);
	}
}
int find_newline(const char *message, int in){
	int i = 0;
	for(i = 0 ; i < in ; i++){
		if(*(message+i) == '\n')
			return i;
	}
	return -1;
}
void echo_all(int sockfd){
	char msg_buf[4096];
	int j = 0;
	printf("[DEBUG] %s: %s\n",connection_list[sockfd]->username, connection_list[sockfd]->buffer);
	for(j = 3 ; j < FD_MAX ; j++){
		if(sockfd != j && serv_sock != j && FD_ISSET(j, &prev)){
			if(!connection_list[j])
				continue;
			snprintf(msg_buf,4096,"[MSG] %s: %s\n", connection_list[sockfd]->username, connection_list[sockfd]->buffer);
			if(write(j,msg_buf,strlen(msg_buf)) <= 0){
				ConnectionClose(j);
			}
			else{
				printf("[DEBUG] %d wrote to %d : %s\n",sockfd,j,connection_list[sockfd]->buffer);
			}
		}
	}

}
int ProcessMessage(int sockfd, char *inbuf, int recv_cnt){
	int i = 0;
	int j = 0;
	int newline_idx= 0;
	if(!connection_list[sockfd])
		return;
	for(i = 0 ; i < recv_cnt ; i++){
		connection_list[sockfd]->buffer[connection_list[sockfd]->buf_idx++] = inbuf[i];
		if(connection_list[sockfd]->buf_idx == 8096){
			connection_list[sockfd]->buffer[8095] = 0x00;
			return BUFFER_FINISH;
		}
	}
	if((newline_idx = find_newline(connection_list[sockfd]->buffer, connection_list[sockfd]->buf_idx)) == -1){
		return BUFFER_REMAINS;
	}
	else{
		return BUFFER_FINISH;
	}
}
void quit(int sockfd){
	if(shutdown(sockfd,SHUT_RD) == -1){
		perror("shutdown error");
		ConnectionClose(sockfd);
	}
	if(write(sockfd, "Goodbye!\n", strlen("Goodbye!\n")) <= 0){
		perror("shutdown write error");
		ConnectionClose(sockfd);
	}
}
void setUsername(int sockfd){
	int i = 0;
	char buf[4096];
	bzero(buf, sizeof(buf));
	strncpy(connection_list[sockfd]->username, connection_list[sockfd]->buffer,64);
	for(i = 3 ; i < FD_MAX ; i++){
		if(i != sockfd && i != serv_sock && FD_ISSET(i, &prev)){
			snprintf(buf, 4096, "User %s entered the room.\n", connection_list[sockfd]->username);
			if(write(i, buf, strlen(buf)) <= 0){
				ConnectionClose(i);
			}
		}
	}
	bzero(buf,sizeof(buf));
	snprintf(buf, 4096, "Hello, %s! Enjoy!\n", connection_list[sockfd]->username);
	if(write(sockfd,buf, strlen(buf)) <= 0){
		ConnectionClose(sockfd);
	}
}
int main(int argc, char *argv[]){
	int port = 8888;
	int optval = 1;
	int i = 0;
	int j = 0;
	fd_set current;
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	bzero((char *)&serv_addr, sizeof(serv_addr));
	setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	serv_addr.sin_family = AF_INET;
	if(argc == 2){
		port = atoi(argv[1]);
	}
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(serv_sock, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) == -1){
		perror("Error on bind");
		exit(1);
	}
	if(listen(serv_sock, 5) == -1){
		perror("Error on listen");
		exit(1);
	}
	printf("[DEBUG] listening on %s:%d\n",inet_ntoa(serv_addr.sin_addr), port);
	FD_ZERO(&prev);
	FD_SET(serv_sock, &prev);
	FD_SET(0, &prev);

	while(true){
		current = prev;
		if(select(FD_MAX, &current, NULL, NULL, NULL) == -1){
			perror("Error on select");
			exit(1);
		}
		for(i = 3 ; i < FD_MAX ; i++){
			if(FD_ISSET(i, &current)){
				//fd:3(listening socket)
				if(i == serv_sock){
					struct sockaddr_in cli_addr;
					socklen_t cli_len = sizeof(cli_addr);
					int newfd;
					newfd = accept(serv_sock, (struct sockaddr *)&cli_addr, &cli_len);
					if(newfd == -1){
						perror("Error on accept");
						exit(1);
					}
					else if(newfd > FD_MAX){
						perror("No more connection can be accepted.");
						close(newfd);
						continue;
					}
					//new fd accepted
					else{
						//can be written, reachable
						if(write(newfd, "Hello!\n", strlen("Hello!\n")) >= 0){
							printf("[DEBUG] new connection on %d at %s:%d\n", newfd, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
							FD_SET(newfd, &prev);
							//add to list
							connection_list[newfd] = (struct s_client *)malloc(sizeof(struct s_client));
							bzero(connection_list[newfd]->username, sizeof(connection_list[newfd]->username));
							bzero(connection_list[newfd]->buffer, sizeof(connection_list[newfd]->buffer));
							connection_list[newfd]->buf_idx = 0;
						}
					}
				}
				//from other fd
				else{	
					char buf[1024];
					char msg_buf[1024];
					bzero(msg_buf,sizeof(msg_buf));
					size_t read_c;
					read_c = read(i, buf, 255);
					//-1 for eof, 0 for nothing
					if(read_c <= 0){
						ConnectionClose(i);
					}	
					else{
						while(ProcessMessage(i, buf, read_c) != BUFFER_FINISH){
						}
						connection_list[i]->buffer[connection_list[i]->buf_idx-1] = 0x00;
						if(connection_list[i]->username[0] == 0x00){
							setUsername(i);

						}
						else if(!strncmp("@list", connection_list[i]->buffer, 5)){
							echoList(i);
						}
						else if(!strncmp("@quit", buf,5)){
							quit(i);
						}
						else if(!strncmp("@id", buf, 3)){
							whisper(i);
						}
						else{
							echo_all(i);
						}
					}
					if(connection_list[i]){
						bzero(connection_list[i]->buffer, sizeof(connection_list[i]->buffer));
						connection_list[i]->buf_idx = 0;
					}
				}
			}
		}
	}
	exit(0);
}
