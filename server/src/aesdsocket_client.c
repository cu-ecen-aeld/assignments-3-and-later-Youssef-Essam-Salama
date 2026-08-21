/*
 * Author: Youssef Essam Salama
 * Date: 2026-08-21
 * Version: 2.0
 * Description: AESD Assignment 6 - Per-client recv/packet assembly,
 *              log file append, and full-file send-back
 */

/*
   ############################################################
   ################# Include libraries ########################
   ############################################################
*/
#include "aesdsocket_client.h"
#include "aesdsocket_common.h"
#include "aesdsocket_socket.h"
#include "aesdsocket_threads.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>

/*
   ############################################################
   ############## Local functions declarations ################
   ############################################################
*/
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

/*
   ############################################################
   ################# Local functions definitions ##############
   ############################################################
*/
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

/*
   ############################################################
   ################# Global functions #########################
   ############################################################
*/
void *handle_client_communication(void *arg)
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
