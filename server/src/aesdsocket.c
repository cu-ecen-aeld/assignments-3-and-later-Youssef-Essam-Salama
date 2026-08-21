/*
 * Author: Youssef Essam Salama
 * Date: 2026-07-29
 * Version: 1.0
 * Description: AESD Assignment 3 - Server
 */

/*
   ############################################################
   ################# Include libraries ########################
   ############################################################
*/
#include "queue.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

/*
   ############################################################
   ###################### Macros ##############################
   ############################################################
*/
#define FALSE (0U)
#define TRUE (1U)
#define SOCKET_PORT "9000"
#define SOCKET_BACKLOG (10U)
#define SOCKET_BUFFER_SIZE (1024U)
#define MAX_PACKET_SIZE (32762U)
#define LOG_FILE_NAME "/var/tmp/aesdsocketdata"

/*
   ############################################################
   ################# Local types ##############################
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
   ################# Local variables ##########################
   ############################################################
*/
static FILE *log_file = NULL;
static int server_sock_fd = -1;
static volatile sig_atomic_t process_running = TRUE;
static uint8_t run_as_daemon = FALSE;
static thread_list_t thread_list;
static pthread_mutex_t log_file_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t thread_list_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
   ############################################################
   ############## Local functions declarations ################
   ############################################################
*/
static uint8_t handle_input_parameters(int argc, char *argv[]);
static void close_socket(int *sock_fd);
static void sig_int_term_handler(int signum);
static uint8_t setup_signal_handlers(void);
static uint8_t create_server_socket(const struct addrinfo *addrinfo);
static uint8_t bind_server_socket(const struct addrinfo *addrinfo);
static uint8_t listen_on_server_socket(void);
static uint8_t setup_socket(void);
static uint8_t setup_log_file(void);
static void try_joining_client_communication_threads(void);
static void close_client_communication_threads_sockets(void);
static void force_joining_client_communication_threads(void);
static void *thread_handler(void *arg);
static uint8_t setup_thread_handler(pthread_t *thread_id);
static uint8_t send_file(int sock_fd, char *buffer, uint32_t buffer_size);
static uint8_t ensure_packet_capacity(char **packet, uint16_t *packet_size,
				      uint16_t packet_index);
static uint8_t flush_packet_to_client(int sock_fd, char *packet,
				      uint16_t packet_len,
				      uint16_t send_buffer_size);
static uint8_t process_received_data(int sock_fd, const char *buffer,
				     int bytes_received, char **packet,
				     uint16_t *packet_index,
				     uint16_t *packet_size);
static void *handle_client_communication(void *arg);
static uint8_t add_thread_to_list(pthread_t thread_id, int sock_fd,
				  const char *client_ip);
static uint8_t create_client_communication_thread(int sock_fd,
						  const char *client_ip);
static uint8_t handle_client_connections(void);
static uint8_t setup_daemon(void);
static uint8_t handle_run_as_daemon(void);
static uint8_t run_server(void);
static void free_resources(void);

/*
   ############################################################
   ################# Local functions definitions ##############
   ############################################################
*/
static uint8_t handle_input_parameters(int argc, char *argv[])
{
	uint8_t ret_val = EXIT_SUCCESS;
	if (argc > 2) {
		syslog(LOG_ERR,
		       "Number of parameters specified greater than needed\n");
		ret_val = EXIT_FAILURE;
	} else if (argc == 2) {
		if (strcmp(argv[1], "-d") == 0) {
			run_as_daemon = TRUE;
			syslog(LOG_INFO, "Running as daemon\n");
		} else {
			syslog(LOG_ERR, "Invalid input parameter: %s\n",
			       argv[1]);
			ret_val = EXIT_FAILURE;
		}
	} else {
		/* Do nothing */
	}

	return ret_val;
}

static void close_socket(int *sock_fd)
{
	shutdown(*sock_fd, SHUT_RDWR);
	close(*sock_fd);
	*sock_fd = -1;
}

static void sig_int_term_handler(int signum)
{
	process_running = FALSE;
	if (server_sock_fd != -1) {
		/* Force main thread break out of accept() system call */
		close_socket(&server_sock_fd);
	}
}

static uint8_t setup_signal_handlers(void)
{
	uint8_t ret_val = EXIT_SUCCESS;

	struct sigaction sig_int_term_action;
	sig_int_term_action.sa_handler = &sig_int_term_handler;
	sigemptyset(&sig_int_term_action.sa_mask);
	sig_int_term_action.sa_flags = 0;

	if (0 != sigaction(SIGINT, &sig_int_term_action, NULL)) {
		syslog(LOG_ERR, "Error setting up SIGINT handler: %s\n",
		       strerror(errno));
		ret_val = EXIT_FAILURE;
	}
	if (0 != sigaction(SIGTERM, &sig_int_term_action, NULL)) {
		syslog(LOG_ERR, "Error setting up SIGTERM handler: %s\n",
		       strerror(errno));
		ret_val = EXIT_FAILURE;
	}

	return ret_val;
}

static uint8_t create_server_socket(const struct addrinfo *addrinfo)
{
	uint8_t ret_val = EXIT_SUCCESS;
	int socket_reuse_option = 1;

	server_sock_fd = socket(addrinfo->ai_family, addrinfo->ai_socktype,
				addrinfo->ai_protocol);
	if (server_sock_fd == -1) {
		syslog(LOG_ERR, "Error creating socket: %s\n", strerror(errno));
		ret_val = EXIT_FAILURE;
	} else if (0 != setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEADDR,
				   &socket_reuse_option,
				   sizeof(socket_reuse_option))) {
		syslog(LOG_ERR, "Error setting socket options: %s\n",
		       strerror(errno));
		ret_val = EXIT_FAILURE;
	} else {
		/* Do nothing */
	}

	return ret_val;
}

static uint8_t bind_server_socket(const struct addrinfo *addrinfo)
{
	uint8_t ret_val = EXIT_SUCCESS;

	if (0 !=
	    bind(server_sock_fd, addrinfo->ai_addr, addrinfo->ai_addrlen)) {
		syslog(LOG_ERR, "Error binding socket: %s\n", strerror(errno));
		ret_val = EXIT_FAILURE;
	}

	return ret_val;
}

static uint8_t listen_on_server_socket(void)
{
	uint8_t ret_val = EXIT_SUCCESS;

	if (0 != listen(server_sock_fd, SOCKET_BACKLOG)) {
		syslog(LOG_ERR, "Error listening on socket: %s\n",
		       strerror(errno));
		ret_val = EXIT_FAILURE;
	}

	return ret_val;
}

static uint8_t setup_socket(void)
{
	uint8_t ret_val = EXIT_SUCCESS;
	int addrinfo_ret;
	struct addrinfo *addrinfo;
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	addrinfo_ret = getaddrinfo(NULL, SOCKET_PORT, &hints, &addrinfo);
	if (addrinfo_ret != 0) {
		syslog(LOG_ERR, "Error getting address info: %s\n",
		       gai_strerror(addrinfo_ret));
		ret_val = EXIT_FAILURE;
	} else {
		ret_val = create_server_socket(addrinfo);
		if (EXIT_SUCCESS == ret_val) {
			ret_val = bind_server_socket(addrinfo);
		}
		if (EXIT_SUCCESS == ret_val) {
			ret_val = listen_on_server_socket();
		}
		freeaddrinfo(addrinfo);
	}

	return ret_val;
}

static uint8_t setup_log_file(void)
{
	uint8_t ret_val = EXIT_SUCCESS;

	log_file = fopen(LOG_FILE_NAME, "w+");
	if (log_file == NULL) {
		syslog(LOG_ERR, "Error opening file: %s\n", strerror(errno));
		ret_val = EXIT_FAILURE;
	}

	return ret_val;
}

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
	while (TRUE == process_running) {
		try_joining_client_communication_threads();
		usleep(1000);
	}
	force_joining_client_communication_threads();
	pthread_exit(NULL);
}

static uint8_t setup_thread_handler(pthread_t *thread_id)
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

static uint8_t send_file(int sock_fd, char *buffer, uint32_t buffer_size)
{
	uint8_t ret_val = EXIT_SUCCESS;
	size_t bytes_read;
	ssize_t bytes_sent;
	size_t total_bytes_sent;

	if (0 != fseek(log_file, 0, SEEK_SET)) {
		syslog(LOG_ERR, "Error seeking to start of file: %s\n",
		       strerror(errno));
		ret_val = EXIT_FAILURE;
	} else {
		do {
			bytes_read = fread(buffer, 1, buffer_size, log_file);
			total_bytes_sent = 0U;
			while (bytes_read > total_bytes_sent) {
				bytes_sent =
					send(sock_fd, buffer + total_bytes_sent,
					     bytes_read - total_bytes_sent, 0);
				if (bytes_sent == -1) {
					syslog(LOG_ERR,
					       "Error sending data: %s\n",
					       strerror(errno));
					ret_val = EXIT_FAILURE;
					break;
				}
				total_bytes_sent += bytes_sent;
			}
		} while (bytes_read > 0);

		if ((EXIT_SUCCESS == ret_val) &&
		    (0 != fseek(log_file, 0, SEEK_END))) {
			syslog(LOG_ERR, "Error seeking to end of file: %s\n",
			       strerror(errno));
			ret_val = EXIT_FAILURE;
		}
	}

	return ret_val;
}

static uint8_t ensure_packet_capacity(char **packet, uint16_t *packet_size,
				      uint16_t packet_index)
{
	uint8_t ret_val = EXIT_SUCCESS;
	char *new_packet;

	if ((packet_index >= *packet_size) &&
	    (packet_index < MAX_PACKET_SIZE)) {
		new_packet = realloc(*packet, (*packet_size) * 2);
		if (new_packet == NULL) {
			syslog(LOG_ERR,
			       "Error reallocating memory for packet: %s\n",
			       strerror(errno));
			ret_val = EXIT_FAILURE;
		} else {
			*packet = new_packet;
			*packet_size *= 2;
		}
	} else if (packet_index >= MAX_PACKET_SIZE) {
		syslog(LOG_ERR, "Packet size exceeded max size\n");
		ret_val = EXIT_FAILURE;
	} else {
		/* Do nothing */
	}

	return ret_val;
}

static uint8_t flush_packet_to_client(int sock_fd, char *packet,
				      uint16_t packet_len,
				      uint16_t send_buffer_size)
{
	uint8_t ret_val = EXIT_SUCCESS;

	pthread_mutex_lock(&log_file_mutex);
	fwrite(packet, 1, packet_len, log_file);
	fflush(log_file);
	ret_val = send_file(sock_fd, packet, send_buffer_size);
	pthread_mutex_unlock(&log_file_mutex);

	if (EXIT_SUCCESS != ret_val) {
		syslog(LOG_ERR, "Error sending file: %s\n", strerror(errno));
	}

	return ret_val;
}

static uint8_t process_received_data(int sock_fd, const char *buffer,
				     int bytes_received, char **packet,
				     uint16_t *packet_index,
				     uint16_t *packet_size)
{
	uint8_t ret_val = EXIT_SUCCESS;
	uint16_t i;

	for (i = 0U; i < (uint16_t)bytes_received; i++) {
		ret_val = ensure_packet_capacity(packet, packet_size,
						 *packet_index);
		if (EXIT_SUCCESS != ret_val) {
			break;
		}

		(*packet)[(*packet_index)++] = buffer[i];
		if (buffer[i] == '\n') {
			ret_val = flush_packet_to_client(
				sock_fd, *packet, *packet_index, *packet_size);
			*packet_index = 0U;
			if (EXIT_SUCCESS != ret_val) {
				break;
			}
		}
	}

	return ret_val;
}

static void *handle_client_communication(void *arg)
{
	uint8_t ret_val = EXIT_SUCCESS;
	thread_node_t *thread_node = (thread_node_t *)arg;
	int sock_fd = thread_node->client_sock_fd;

	char *packet = NULL;
	char *buffer = NULL;
	int bytes_received = 0;
	uint16_t packet_index = 0U;
	uint16_t packet_size = SOCKET_BUFFER_SIZE;

	packet = malloc(SOCKET_BUFFER_SIZE);
	buffer = malloc(SOCKET_BUFFER_SIZE);
	if (packet == NULL || buffer == NULL) {
		syslog(LOG_ERR,
		       "Error allocating memory for packet or buffer: %s\n",
		       strerror(errno));
	} else {
		do {
			bytes_received =
				recv(sock_fd, buffer, SOCKET_BUFFER_SIZE, 0);
			if (bytes_received > 0) {
				ret_val = process_received_data(
					sock_fd, buffer, bytes_received,
					&packet, &packet_index, &packet_size);
				if (EXIT_SUCCESS != ret_val) {
					bytes_received = 0;
				}
			}
		} while (bytes_received > 0 && process_running == TRUE);
	}

	if (packet != NULL) {
		free(packet);
	}
	if (buffer != NULL) {
		free(buffer);
	}
	if (bytes_received == -1 && process_running == TRUE) {
		syslog(LOG_ERR, "Error receiving data: %s\n", strerror(errno));
	}

	pthread_mutex_lock(&thread_list_mutex);
	if (thread_node->client_sock_fd != -1) {
		close_socket(&thread_node->client_sock_fd);
	}
	syslog(LOG_INFO, "Closed connection from %s\n", thread_node->client_ip);
	thread_node->thread_completed = TRUE;
	pthread_mutex_unlock(&thread_list_mutex);

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

static uint8_t create_client_communication_thread(int sock_fd,
						  const char *client_ip)
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

static uint8_t handle_client_connections(void)
{
	uint8_t ret_val = EXIT_SUCCESS;
	struct sockaddr client_addr;
	socklen_t client_addr_len;
	char *client_ip;
	int accepted_sock_fd;

	while (TRUE == process_running) {
		client_addr_len = sizeof(client_addr);
		accepted_sock_fd =
			accept(server_sock_fd, &client_addr, &client_addr_len);
		if (accepted_sock_fd == -1) {
			if (FALSE == process_running) {
				break; /* SIGINT/SIGTERM caught while waiting
					  for connection */
			} else {
				syslog(LOG_ERR,
				       "Error accepting connection: %s\n",
				       strerror(errno));
				ret_val = EXIT_FAILURE;
				break;
			}
		}
		client_ip = inet_ntoa(
			((struct sockaddr_in *)&client_addr)->sin_addr);
		syslog(LOG_INFO, "Accepted connection from %s\n", client_ip);

		ret_val = create_client_communication_thread(accepted_sock_fd,
							     client_ip);
		if (EXIT_SUCCESS != ret_val) {
			break;
		}
	}

	if (EXIT_SUCCESS == ret_val) {
		syslog(LOG_INFO, "Caught signal, exiting\n");
	}

	process_running = FALSE;
	return ret_val;
}
static uint8_t setup_daemon(void)
{
	uint8_t ret_val = EXIT_SUCCESS;
	int null_fd;

	/* Redirect stdin, stdout, stderr to /dev/null */
	if (-1 == setsid()) {
		syslog(LOG_ERR, "Error setting up session: %s\n",
		       strerror(errno));
		ret_val = EXIT_FAILURE;
	}
	if ((EXIT_SUCCESS == ret_val) && (0 != chdir("/"))) {
		syslog(LOG_ERR, "Error changing directory: %s\n",
		       strerror(errno));
		ret_val = EXIT_FAILURE;
	}
	if (EXIT_SUCCESS == ret_val) {
		null_fd = open("/dev/null", O_RDWR);
		if (null_fd == -1) {
			syslog(LOG_ERR, "Error opening /dev/null: %s\n",
			       strerror(errno));
			ret_val = EXIT_FAILURE;
		} else {
			dup2(null_fd, STDIN_FILENO);
			dup2(null_fd, STDOUT_FILENO);
			dup2(null_fd, STDERR_FILENO);
			if (null_fd > STDERR_FILENO) {
				close(null_fd);
			}
		}
	}

	return ret_val;
}

static uint8_t handle_run_as_daemon(void)
{
	pid_t pid;
	uint8_t ret_val = EXIT_SUCCESS;

	if (TRUE == run_as_daemon) {
		pid = fork();
		if (pid == -1) {
			syslog(LOG_ERR, "Error forking: %s\n", strerror(errno));
			ret_val = EXIT_FAILURE;
		} else if (pid != 0) {
			/* Parent process, exit */
			_exit(EXIT_SUCCESS);
		} else {
			/* Child process, setup daemon */
			ret_val = setup_daemon();
		}
	}

	return ret_val;
}

static uint8_t run_server(void)
{
	uint8_t ret_val = EXIT_SUCCESS;
	pthread_t thread_handler_id;

	ret_val = setup_thread_handler(&thread_handler_id);
	if (EXIT_SUCCESS == ret_val) {
		ret_val = handle_client_connections();
		pthread_join(thread_handler_id, NULL);
	}

	return ret_val;
}

static void free_resources(void)
{
	if (server_sock_fd != -1) {
		syslog(LOG_INFO, "Closing socket\n");
		close_socket(&server_sock_fd);
	}

	if (log_file != NULL) {
		syslog(LOG_INFO, "Closing log file\n");
		fclose(log_file);
		log_file = NULL;
		syslog(LOG_INFO, "Deleting log file\n");
		unlink(LOG_FILE_NAME);
	}
}

/*
   ############################################################
   ################# Global functions #########################
   ############################################################
*/
int main(int argc, char *argv[])
{
	uint8_t ret_val = EXIT_SUCCESS;

	do {
		openlog(NULL, 0, LOG_USER);

		ret_val = handle_input_parameters(argc, argv);
		if (EXIT_SUCCESS != ret_val) {
			break;
		}

		ret_val = setup_socket();
		if (EXIT_SUCCESS != ret_val) {
			break;
		}

		ret_val = setup_log_file();
		if (EXIT_SUCCESS != ret_val) {
			break;
		}

		ret_val = handle_run_as_daemon();
		if (EXIT_SUCCESS != ret_val) {
			break;
		}

		ret_val = setup_signal_handlers();
		if (EXIT_SUCCESS != ret_val) {
			break;
		}

		ret_val = run_server();
	} while (0);

	free_resources();
	exit(ret_val);
}
