/*
 * Author: Youssef Essam Salama
 * Date: 2026-07-29
 * Version: 1.0
 * Description: AESD Assignment 3 - Client thread list and joiner
 */

#include "aesdsocket_threads.h"
#include "aesdsocket_client.h"
#include "aesdsocket_socket.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

/*
   ############################################################
   ############## Local functions declarations ################
   ############################################################
*/
static void try_joining_client_communication_threads(void);
static void close_client_communication_threads_sockets(void);
static void force_joining_client_communication_threads(void);
static void *thread_handler(void *arg);
static uint8_t add_thread_to_list(pthread_t thread_id, int sock_fd,
				  const char *client_ip);

/*
   ############################################################
   ################# Global variables #########################
   ############################################################
*/
thread_list_t thread_list;
pthread_mutex_t thread_list_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
   ############################################################
   ################# Local functions definitions ##############
   ############################################################
*/
static void try_joining_client_communication_threads(void)
{
	thread_node_t *current_thread_node;
	thread_node_t *next_thread_node;

	pthread_mutex_lock(&thread_list_mutex);
	SLIST_FOREACH_SAFE(current_thread_node, &thread_list, next_thread_node,
			   next_thread_node)
	{
		if (TRUE == current_thread_node->thread_completed) {
			pthread_join(current_thread_node->thread_id, NULL);
			SLIST_REMOVE(&thread_list, current_thread_node,
				     thread_node, next_thread_node);
			free(current_thread_node);
		}
	}
	pthread_mutex_unlock(&thread_list_mutex);
}

static void close_client_communication_threads_sockets(void)
{
	thread_node_t *current_thread_node;
	thread_node_t *next_thread_node;

	pthread_mutex_lock(&thread_list_mutex);
	SLIST_FOREACH_SAFE(current_thread_node, &thread_list, next_thread_node,
			   next_thread_node)
	{
		/* Force client communication thread break out of recv() system
		 * call */
		close_socket(&current_thread_node->client_sock_fd);
	}
	pthread_mutex_unlock(&thread_list_mutex);
}

static void force_joining_client_communication_threads(void)
{
	uint8_t thread_list_empty = FALSE;
	close_client_communication_threads_sockets();
	do {
		try_joining_client_communication_threads();
		pthread_mutex_lock(&thread_list_mutex);
		thread_list_empty = SLIST_EMPTY(&thread_list);
		pthread_mutex_unlock(&thread_list_mutex);
		if (thread_list_empty == FALSE) {
			usleep(1000);
		}
	} while (thread_list_empty == FALSE);
}

static void *thread_handler(void *arg)
{
	(void)arg;
	while (TRUE == process_running) {
		try_joining_client_communication_threads();
		usleep(1000);
	}
	force_joining_client_communication_threads();
	pthread_exit(NULL);
}

static uint8_t add_thread_to_list(pthread_t thread_id, int sock_fd,
				  const char *client_ip)
{
	uint8_t ret_val = EXIT_SUCCESS;
	thread_node_t *thread_node;
	thread_node = malloc(sizeof(thread_node_t));
	if (thread_node == NULL) {
		syslog(LOG_ERR, "Error allocating memory for thread node: %s\n",
		       strerror(errno));
		ret_val = EXIT_FAILURE;
	} else {
		thread_node->thread_id = thread_id;
		thread_node->thread_completed = FALSE;
		thread_node->client_sock_fd = sock_fd;
		strncpy(thread_node->client_ip, client_ip,
			sizeof(thread_node->client_ip) - 1U);
		thread_node->client_ip[sizeof(thread_node->client_ip) - 1U] =
			'\0';
		SLIST_INSERT_HEAD(&thread_list, thread_node, next_thread_node);
	}

	return ret_val;
}

/*
   ############################################################
   ################# Global functions #########################
   ############################################################
*/
uint8_t setup_thread_handler(pthread_t *thread_id)
{
	uint8_t ret_val = EXIT_SUCCESS;

	ret_val = pthread_create(thread_id, NULL, &thread_handler, NULL);
	if (ret_val != 0) {
		syslog(LOG_ERR, "Error creating thread handler: %s\n",
		       strerror(ret_val));
		ret_val = EXIT_FAILURE;
	}

	return ret_val;
}

uint8_t create_client_communication_thread(int sock_fd, const char *client_ip)
{
	uint8_t ret_val = EXIT_SUCCESS;
	pthread_t thread_id = 0;
	thread_node_t *thread_node = NULL;

	pthread_mutex_lock(&thread_list_mutex);
	do {
		ret_val = add_thread_to_list(thread_id, sock_fd, client_ip);
		if (EXIT_SUCCESS != ret_val) {
			ret_val = EXIT_FAILURE;
			break;
		}
		thread_node = SLIST_FIRST(&thread_list);

		ret_val = pthread_create(&thread_id, NULL,
					 &handle_client_communication,
					 (void *)thread_node);
		if (ret_val != 0) {
			syslog(LOG_ERR, "Error creating thread: %s\n",
			       strerror(ret_val));
			SLIST_REMOVE_HEAD(&thread_list, next_thread_node);
			free(thread_node);
			ret_val = EXIT_FAILURE;
			break;
		}

		thread_node->thread_id = thread_id;
	} while (0);
	pthread_mutex_unlock(&thread_list_mutex);

	if (EXIT_SUCCESS != ret_val) {
		close_socket(&sock_fd);
	}

	return ret_val;
}
