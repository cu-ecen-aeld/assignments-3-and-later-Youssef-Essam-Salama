#ifndef AESDSOCKET_COMMON_H
#define AESDSOCKET_COMMON_H

/*
   ############################################################
   ################# Include libraries ########################
   ############################################################
*/
#include "queue.h"
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>

/*
   ############################################################
   ###################### Macros ##############################
   ############################################################
*/
#define USE_AESD_CHAR_DEVICE (1U)

#define FALSE (0U)
#define TRUE (1U)
#define SOCKET_PORT "9000"
#define SOCKET_BACKLOG (10U)
#define SOCKET_BUFFER_SIZE (1024U)
#define MAX_PACKET_SIZE (32762U)

#if (USE_AESD_CHAR_DEVICE == 1U)
#define LOG_FILE_NAME "/dev/aesdchar"
#else
#define LOG_FILE_NAME "/var/tmp/aesdsocketdata"
#endif

/*
   ############################################################
   ################# Global types #############################
   ############################################################
*/
struct thread_node {
	pthread_t thread_id;
	sig_atomic_t thread_completed;
	int client_sock_fd;
	char client_ip[INET_ADDRSTRLEN];
	SLIST_ENTRY(thread_node) next_thread_node;
};

typedef struct thread_node thread_node_t;
typedef SLIST_HEAD(thread_list, thread_node) thread_list_t;

/*
   ############################################################
   ################# Global variables #########################
   ############################################################
*/
extern FILE *log_file;
extern volatile sig_atomic_t process_running;
extern pthread_mutex_t log_file_mutex;

#endif /* AESDSOCKET_COMMON_H */
