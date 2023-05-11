/**
 * @file aesdsocket.c
 * @brief This file is to be used to Linux 
 * Kernel Programming and Introduction to Yocto Project.
 *
 * @author Iosif Futerman
 * @date April 10, 2023
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <syslog.h>
#include <signal.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include "aesdsocket.h"
#include "../aesd-char-driver/aesd_ioctl.h"

#define PORT "9000"  // the port users will be connecting to

#define BACKLOG 10   // how many pending connections queue will hold

#ifndef USE_AESD_CHAR_DEVICE
#define USE_AESD_CHAR_DEVICE 1
#endif

#if !USE_AESD_CHAR_DEVICE
#define FILEPATH "/var/tmp/aesdsocketdata"
#else
#define FILEPATH "/dev/aesdchar"
#endif

#define BUFSIZE 512

volatile static int work_state = 1;

//int g_fd, g_sfd;//File descriptors for aesdsocketdata file, socket and connection
int g_sfd;//File descriptors for aesdsocketdata file, socket and connection
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static thr_node* g_head = NULL;
static timer_t g_timer;

int main(int argc, char** argv){    
	openlog(NULL, LOG_CONS | LOG_PID, LOG_INFO);
	syslog(LOG_INFO, "Initialise server");
	
	if(init_server(argc, argv)){
		closelog();
		return -1;
	}
	
	struct sigaction action;
	memset(&action, 0, sizeof(sigaction));
	action.sa_handler = signal_handler;
	sigemptyset(&action.sa_mask); 
	action.sa_flags = 0;
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);
	
	
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
  
  g_sfd = init_socket();
  sockfd = g_sfd;
  
  if(sockfd == -1){
		syslog(LOG_ERR, "init_socket FAILED");
		closelog();
		return -1;
  }
  
	syslog(LOG_INFO, "Socked inited sockfd=%d", sockfd);
	
  if (listen(sockfd, BACKLOG)) {
		syslog(LOG_ERR, "listen FAILED");
		closelog();
    return -1;
  }
  
  struct sockaddr_storage their_addr;
  socklen_t addr_size  = sizeof their_addr;
//  int fflags = O_RDWR | O_APPEND | O_CREAT | O_TRUNC;
  

  thr_node* last = NULL;
  while(work_state){
  	syslog(LOG_INFO, "Wait for connection");
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
		if(new_fd == -1){
			syslog(LOG_INFO, "accept FAILED");
			break;
		}
		syslog(LOG_INFO, "Socket accepted");
		char* s = malloc(INET6_ADDRSTRLEN);
    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, INET6_ADDRSTRLEN);
    syslog(LOG_INFO, "Accepted connection from %s; new_fd: %d", s, new_fd);
  	
  	struct proc_data * data = malloc(sizeof(struct proc_data));
//  	data->fd = fd;
  	data->sd = new_fd;
  	data->address = s;
  	pthread_t thr;
  	
  	pthread_create(&thr, NULL, connection_processor, (void*)data);
  	
  	thr_node* current = malloc(sizeof(thr_node));
  	current->thr = thr;
  	current->next = NULL;
  	if(g_head == NULL){
  		g_head = current;
  	}
  	else{
  		last->next = current;
  	}
		last = current;
  }
  deinit();
  exit (EXIT_SUCCESS);
}

void deinit(){
	deinit_timer();
	thr_node* current = g_head;
	while(current != NULL){
		struct proc_data* data;
		pthread_join(current->thr, ((void**)&data));
		
		free(data->address);
		close(data->sd);
		free(data);
		
		thr_node* next = current->next;
		free(current);
		current = next;
	}
	close(g_sfd);
//	close(g_fd);
	if(!USE_AESD_CHAR_DEVICE){
		unlink(FILEPATH);
	}
	
}

int write_to_file(int fd, const char* buf, size_t n_byte){
	int res, offset = 0, length = n_byte;
	while(work_state){
		res = write(fd, buf + offset, length);
		if(res == -1){
  		syslog(LOG_ERR, "write FAILED error:%s", strerror(errno));
			return -1;
		}
		offset += res;
		length -= res;
		if(offset == n_byte){
			break;
		}
	}
	return 0;
}

int send_to_socket(int sockfd, char* buf, size_t n_byte){
	int res, offset = 0, length = n_byte;
	while(work_state){
		res = send(sockfd, buf + offset, length, 0);
		if(res == -1){
  		syslog(LOG_ERR, "send FAILED");
			return -1;
		}
		offset += res;
		length -= res;
		if(offset == n_byte){
			break;
		}
	}
	return 0;
}

int send_from_file(int fd, int sockfd){
	char buf[BUFSIZE];
	int res, offset = 0; 
		
	while(work_state){
		res = pread(fd, buf, BUFSIZE, offset);
		if(res == -1){
			syslog(LOG_ERR, "read FAILED");
			return -1;
		}
		if(!res){
			break;
		}
		if(send_to_socket(sockfd, buf, res)){
			syslog(LOG_ERR, "send_to_socket FAILED");		
			return -1;
		}
		offset += res;
	}
	return 0;
}

int parse_seek_to(char* buf, uint32_t* write_cmd, uint32_t* write_cmd_offset){
	int32_t res;
	buf = strtok(buf, ":");
	res = strtol(strtok(NULL, ","), NULL, 10);
	if(res < 0){
		return -1;
	}
	*write_cmd = res;
	res = strtol(strtok(NULL, "\n"), NULL, 10);
	if(res < 0){
		return -1;
	}
	*write_cmd_offset = res;
	return 0;
}

int recieve_to_file(int fd, int sockfd){

  int res; 
  char buf[BUFSIZE];
  char *start_cursor;
	int flags = 0;
//	char lastChar;
  
  while(work_state){
    res = recv(sockfd, buf, BUFSIZE, flags);
    flags = MSG_DONTWAIT;
    if(res == -1){
    	if(errno == EAGAIN || errno == EWOULDBLOCK){
    		break;
    	}
  		syslog(LOG_ERR, "recv FAILED");
  		syslog(LOG_ERR, "recv FAILED error:%s", strerror(errno));
		  return -1;
    }
    if(res == 0){
    	break;
    }
//    lastChar = buf[res - 1];
		start_cursor = strstr(buf, "AESDCHAR_IOCSEEKTO:");
		if(start_cursor){
			syslog(LOG_INFO, "COMMAND founded! COMMAND:%s\n", buf);
			struct aesd_seekto cmd;
			if(!parse_seek_to(buf, &cmd.write_cmd, &cmd.write_cmd_offset)){
				syslog(LOG_INFO, "COMMAND parsed! write_cmd:%d;write_cmd_offset:%d\n", cmd.write_cmd, cmd.write_cmd_offset);
				res = ioctl(fd, AESDCHAR_IOCSEEKTO, &cmd);
				if(res < 0){
					syslog(LOG_INFO, "IOCTL FAILED res:%d\n", res);
					return -1;
				}
				return 0;
			}
		}
  	res = write_to_file(fd, buf, res);
  	if(res){
			syslog(LOG_ERR, "write_to_file FAILED");
    	return -1;
  	}
  }
  /*if(lastChar != '\n'){
  	res = write_to_file(fd, "\n", res);
  	if(res){
			syslog(LOG_ERR, "write_to_file FAILED");
    	return -1;
  	}
  }*/
  return 0;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int init_server(int argc, char** argv){
	if(argc >= 2){
		syslog(LOG_INFO, "Daemon mode");
		if(!strcmp(argv[1], "-d")){
			pid_t pid;
			pid = fork();
			if(pid == -1){
				syslog(LOG_ERR, "fork FAILED");
				return -1;
			}		
			if(pid){
				syslog(LOG_INFO, "Daemon started with PID = %d", pid);
				closelog();
				exit(EXIT_SUCCESS);
			}
		}
		if(setsid() == -1){
			syslog(LOG_ERR, "setsid FAILED");
			return -1;
		}
		if(chdir("/") == -1){
			syslog(LOG_ERR, "chdir FAILED");
			return -1;
		}
		for (int i = 0; i < 3; i++){
			close (i);
		}
		open ("/dev/null", O_RDWR); /* stdin */
		dup (0); /* stdout */
		dup (0); /* stderror */
	}
	else{
		syslog(LOG_INFO, "Proces mode");
	}
	init_timer();
	return 0;
}

int init_socket(){
	struct addrinfo hints, *servinfo, *p;
  
  int yes=1, sockfd;
  
//	hints.ai_family = AF_UNSPEC;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;


  if (getaddrinfo(NULL, PORT, &hints, &servinfo)) {
  	syslog(LOG_ERR, "getaddrinfo FAILED");
		return -1;
  }
  
  for(p = servinfo; p != NULL; p = p->ai_next) {
  	if(p->ai_family != AF_INET){
  		continue;
  	}
    syslog(LOG_INFO, "Try to get socket p->ai_family %d; p->ai_socktype %d; p->ai_protocol %d", p->ai_family, p->ai_socktype,	p->ai_protocol);
	  sockfd = socket(p->ai_family, p->ai_socktype,	p->ai_protocol);
    if (sockfd == -1) {
	  	syslog(LOG_WARNING, "socket FAILED");
	    continue;
	  }
		
	  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) {
			syslog(LOG_ERR, "setsockopt FAILED");
			return -1;
	  }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen)) {
			close(sockfd);
			syslog(LOG_WARNING, "bind FAILED");
      continue;
    }

    break;
	}
	
	freeaddrinfo(servinfo);
	
	if (p == NULL)  {
		syslog(LOG_ERR, "No one socket are opened");
		return -1;
  }
  syslog(LOG_INFO, "Socket recieved");
  return sockfd;
}

void signal_handler(int signo){
	syslog(LOG_INFO, "Caught signal, exiting");
	work_state = 0;
	close(g_sfd);
}

void* connection_processor(void* arg){
	struct proc_data* data = (struct proc_data*)arg;
	int fd;
	int fflags = O_RDWR | O_APPEND | O_CREAT | O_TRUNC;
  fd = open(FILEPATH, fflags, 0666);
  if(fd == -1){
		syslog(LOG_ERR, "open FAILED error:%s", strerror(errno));
	  return arg;
	}
	pthread_mutex_lock(&mutex);
	int res = recieve_to_file(fd, data->sd);
	pthread_mutex_unlock(&mutex);
	
  if(res || !work_state){
		close(data->sd);
		close(fd);
		pthread_exit(arg);
	}	
	
	pthread_mutex_lock(&mutex);
	send_from_file(fd, data->sd);
	pthread_mutex_unlock(&mutex);
	close(fd);
//	close(data->sd);
  syslog(LOG_INFO, "Closed connection from %s", data->address);
  return arg;
//	pthread_exit(arg);
}

void init_timer(){
	if(USE_AESD_CHAR_DEVICE)
	{
		return;
	}
	struct sigevent event;
	memset(&event, 0, sizeof(struct sigevent));
	event.sigev_notify = SIGEV_THREAD;
	event.sigev_notify_function = &timer_handler;
	event.sigev_value.sival_ptr = &g_timer;
	int res = timer_create(CLOCK_REALTIME, &event, &g_timer);
	syslog(LOG_INFO, "Timer reated with res %d", res);
	struct timespec spec;
	memset(&spec, 0, sizeof(struct timespec));
	spec.tv_sec = 10;
	spec.tv_nsec = 0;
	struct itimerspec set;
	set.it_interval = spec;
	set.it_value = spec;
	res = timer_settime(g_timer, 0, &set, NULL);
	syslog(LOG_INFO, "Timer setted with res %d", res);
}
void deinit_timer(){
	if(USE_AESD_CHAR_DEVICE)
	{
		return;
	}
	struct itimerspec set;
	memset(&set, 0, sizeof(	struct itimerspec));
	timer_settime(g_timer, 0, &set, NULL);
}

void timer_handler(sigval_t val){
	if(USE_AESD_CHAR_DEVICE)
	{
		return;
	}
	int fflags = O_RDWR | O_APPEND | O_CREAT | O_TRUNC;
	int fd;
  fd = open(FILEPATH, fflags, 0666);
  if(fd == -1){
		syslog(LOG_ERR, "open FAILED error:%s", strerror(errno));
		return;
	}
	time_t now;
	now = time(NULL);
	struct tm* time = localtime(&now);
	char* format = "timestamp:%a, %d %b %Y %T %z\n";
	char str_time[50];
	size_t str_size = strftime(str_time, 50, format, time);
	syslog(LOG_INFO, "watchdog %s", str_time);
	printf("WATCHDOG %s", str_time);
	pthread_mutex_lock(&mutex);
	write_to_file(fd, str_time, str_size);
	pthread_mutex_unlock(&mutex);
	close(fd);
}
