/**
 * @file aesdsocket.h 
 * @brief This file is to be used to Linux 
 * Kernel Programming and Introduction to Yocto Project.
 *
 * @author Iosif Futerman
 * @date April 10, 2023
 *
 */

/**
 * @brief This function converts a sockaddr structure
 * to a sockaddr_in structure
 *
 * @param sa pointer to a sockaddr_in structure
 * 
 * @return 
 */
void *get_in_addr(struct sockaddr *sa);
/**
 * @brief This function is competely writing char buffer to a file
 *
 * @param fd destination file descriptor
 * @param buf source char buffer
 * @param n_byte count of chars to write
 * @return success status 0 - success
 */
int write_to_file(int fd, const char* buf, size_t n_byte);
/**
 * @brief This function initialise socket server in process or daemon mode
 *
 * @param argc count of arguments. Usualy from the main function
 * @param argv pointer to strings with arguments. Usualy from the main function
 * @return success status 0 - success
 */
int init_server(int argc, char** argv);
/**
 * @brief This function initialise socket listener in the server side
 *
 * @return success status 0 - success
 */
int init_socket();
/**
 * @brief This function recieves data from a given socket and writes data to a given file
 *
 * @param fd destination file descriptor
 * @param sockfd source socket descriptor
 * @return success status 0 - success
 */
int recieve_to_file(int fd, int sockfd);
/**
 * @brief This function reads data from a given file and sends data to a given socket
 *
 * @param sockfd destination socket descriptor
 * @param fd destination file descriptor
 * @return success status 0 - success
 */
int send_from_file(int fd, int sockfd);
/**
 * @brief This function competely sends a char buffer to a given socket
 *
 * @param sockfd destination socket descriptor
 * @param buf source char buffer
 * @param n_byte count of chars to send
 * @return success status 0 - success
 */
int send_to_socket(int sockfd, char* buf, size_t n_byte);
/**
 * @brief This is the signal handler for the "nice" completing of the process
 *
 * @param signo number of recieved signal
 * @return void
 */
void signal_handler(int signo);
/**
 * @brief This is the thread function for realizing recive\send logic
 *
 * @param arg must be a pointer to the proc_data structure
 * @return returns a pointer to the intial proc_data structure
 */
void* connection_processor(void* arg);
/**
 * @brief This function deinitialise socket server
 *
 * @return void
 */
void deinit();
/**
 * @brief This function initialise and arms the timer for the 10 sec interval
 *
 * @return void
 */
void init_timer();
/**
 * @brief This function disarms the timer
 *
 * @return void
 */
void deinit_timer();

int parse_seek_to(char* buf, uint32_t *write_cmd, uint32_t *write_cmd_offset);

/**
 * @brief This function calls by kerner each 10 seconds, if init_timer was called
 * This function appends tiimestamps to the file with conversation log
 * @return void
 */
void timer_handler(sigval_t val);
/*
 * Structure for sending data to the thread with socket handling
 */
struct proc_data{
//	int fd; /*File descriptor for a log file*/
	int sd; /*Socket descriptor*/
	char* address;
};
/*
 * Structure for Connected list implementation
 */
struct thr_node_s {
	pthread_t thr; /* Thread descriptor*/
	struct thr_node_s* next; /* Pointer to the next node. NULL if node is not exists*/
} thr_node_s_default = {0, NULL};
typedef struct thr_node_s thr_node;
