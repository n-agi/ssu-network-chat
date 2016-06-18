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
	char username[64];
} *connection_list[FD_MAX];
int serv_sock;
struct sockaddr_in serv_addr;
fd_set prev;
void ConnectionClose(int fd){
	//logout message
	char logout[512];
	int j = 0;
	snprintf(logout, 512,"User %s disconnected.\n",connection_list[fd]->username);
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
		for(i = 0 ; i < FD_MAX ; i++){
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
						if(write(newfd, "What is your name?\n", strlen("What is your name?\n")) >= 0){
							printf("[DEBUG] new connection on %d at %s:%d\n", newfd, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
							FD_SET(newfd, &prev);
							//add to list
							connection_list[newfd] = (struct s_client *)malloc(sizeof(struct s_client));
							bzero(connection_list[newfd]->username, sizeof(connection_list[i]->username));
						}
					}
				}
				//from other fd
				else{
					
					char buf[4096];
					char msg_buf[4096];
					bzero(msg_buf,sizeof(msg_buf));
					size_t read_c;
					read_c = read(i, buf, 255);
					//-1 for eof, 0 for nothing
					if(read_c <= 0){
						ConnectionClose(i);
					}	
					else{
						//printf("%s",buf);
						if(!connection_list[i]){
							continue;
						}
						buf[read_c-1] = 0x00;
						if(connection_list[i]->username[0] == 0x00){
							strncpy(connection_list[i]->username, buf, 64);
							printf("[DEBUG] user from %d set his name to %s\n",i,connection_list[i]->username);
							for(j = 3 ; j < FD_MAX ; j++){
								if(!connection_list[j])
									continue;
								if(serv_sock != j && FD_ISSET(j, &prev)){
									snprintf(msg_buf,4096,"%s has join the room.\n",connection_list[j]->username);
									if(write(j, msg_buf, strlen(msg_buf)) <= 0){
										ConnectionClose(j);
									}
								}
							}
							if(write(i, "Enjoy chat!\n", strlen("Enjoy chat!\n")) <= 0){
								ConnectionClose(i);
							}
						}
						//this is chat
						else{
							if(!strncmp("@list", buf, 5)){
								snprintf(msg_buf,4096,"%s\n", "--User List--");
								if(write(i, msg_buf,strlen(msg_buf)) <= 0){
									ConnectionClose(i);

								}
								for(j = 3 ; j < FD_MAX ; j++){
									if(!connection_list[j])
										continue;
									if(serv_sock != j && FD_ISSET(j, &prev)){
										snprintf(msg_buf,4096,"%s\n",connection_list[j]->username);
										if(write(i, msg_buf, strlen(msg_buf)) <= 0){
											ConnectionClose(i);
										}
									}
								}
								snprintf(msg_buf,4096,"%s\n","-------------");
								if(write(i, msg_buf, strlen(msg_buf)) <= 0){
									ConnectionClose(i);
								}
							}
							else if(!strncmp("@id", buf, 3)){

							}
							else if(!strncmp("@quit", buf,5)){
								if(shutdown(i,SHUT_RD) == -1){
									perror("shutdown error");
									ConnectionClose(i);
								}
								if(write(i, "Goodbye!\n", strlen("Goodbye!\n")) <= 0){
									perror("shutdown write error");
									ConnectionClose(i);
								}
							}
							else if(!strncmp("@message", buf, 8)){
								char *id = strchr(buf, ' ')+1;
								char *msg;
								bool found=false;
								if(id == NULL){
									snprintf(msg_buf,4096,"Argument is wrong!\n");	
									if(write(i, msg_buf, strlen(msg_buf)) <= 0){
										ConnectionClose(i);
									}
									continue;
								}
								else{
									char *end = strchr(id, ' ');
									if(end == NULL){
										snprintf(msg_buf, 4096, "Argument is wrong!\n");
										if(write(i, msg_buf, strlen(msg_buf)) <= 0){
											ConnectionClose(i);
										}
										continue;
									}
									else{
										*end = 0x00;
										msg = end+1;
										for(j = 3 ; j < FD_MAX ; j++){
											if(serv_sock != j && FD_ISSET(j, &prev)){
												if(!strncmp(connection_list[j]->username, id, (end-id)) && (strlen(connection_list[j]->username) == strlen(id))){				
		                                                	                                if(i == j){
														snprintf(msg_buf,4096,"You are whispering to yourself!\n");
		                                        	                                                if(write(i,msg_buf,strlen(msg_buf)) <= 0){
		                                	                                                                ConnectionClose(i);
		                        	                                                                }      
														found=true;
														continue;
                			                                                                }      
													found = true;
													snprintf(msg_buf,4096,"%s->%s: %s\n", connection_list[i]->username, connection_list[j]->username, msg);
													if(write(j,msg_buf,strlen(msg_buf)) <= 0){
														ConnectionClose(j);
													}
													if(write(i,msg_buf,strlen(msg_buf)) <= 0){
														ConnectionClose(j);
													}
												}
											}
										}
										if(!found){
											if(write(i,"User not found!\n", strlen("User not found!\n")) <= 0){
												ConnectionClose(i);
											}
										}
									}
								}
							}
							else{
								printf("[DEBUG] %s: %s\n",connection_list[i]->username, buf);
								for(j = 3 ; j < FD_MAX ; j++){
									if(i != j && serv_sock != j && FD_ISSET(j, &prev)){
										if(!connection_list[i])
											continue;
										snprintf(msg_buf,4096,"[MSG] %s: %s\n", connection_list[i]->username, buf);
										if(write(j,msg_buf,strlen(msg_buf)) < 0){
											ConnectionClose(j);
										}
										else{
											printf("[DEBUG] %d wrote to %d : %s\n",i,j,msg_buf);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	exit(0);
}
