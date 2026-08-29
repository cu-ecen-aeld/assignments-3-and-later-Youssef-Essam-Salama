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

/*
   ############################################################
   ###################### Macros ##############################
   ############################################################
*/
#define FALSE (0U)
#define TRUE (1U)

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
extern volatile sig_atomic_t process_running;

#endif /* AESDSOCKET_COMMON_H */
