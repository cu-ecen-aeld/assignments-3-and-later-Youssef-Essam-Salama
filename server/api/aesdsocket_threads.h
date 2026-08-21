#ifndef AESDSOCKET_THREADS_H
#define AESDSOCKET_THREADS_H

/*
   ############################################################
   ################# Include libraries ########################
   ############################################################
*/
#include "aesdsocket_common.h"
#include <stdint.h>

/*
   ############################################################
   ################# Global variables #########################
   ############################################################
*/
extern thread_list_t thread_list;
extern pthread_mutex_t thread_list_mutex;

/*
   ############################################################
   ################# Global functions #########################
   ############################################################
*/
uint8_t setup_thread_handler(pthread_t *thread_id);
uint8_t create_client_communication_thread(int sock_fd, const char *client_ip);
uint8_t setup_time_stamping_thread(pthread_t *thread_id);

#endif /* AESDSOCKET_THREADS_H */
