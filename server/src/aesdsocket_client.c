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
#include "aesd_ioctl.h"
#include "aesdsocket_cfg.h"
#include "aesdsocket_common.h"
#include "aesdsocket_logging.h"
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
static uint8_t send_file(int sock_fd, char *buffer, uint32_t buffer_size,
			 long seek_pos);
static uint8_t ensure_packet_capacity(char **packet, uint16_t *packet_size,
				      uint16_t packet_index);
static uint8_t convert_str_to_long(const char *str, long *conversion_result,
				   long min_value, long max_value);
static uint8_t parse_x_str_and_y_str(const char *xy_str,
				     struct aesd_seekto *seekto_cmd);
static uint8_t parse_xy_str(const char *packet, uint16_t packet_length,
			    struct aesd_seekto *seekto_cmd);
static uint8_t is_seekto_packet(char *packet, uint16_t packet_length,
				struct aesd_seekto *seekto_cmd);
static uint8_t process_seekto_packet(int sock_fd,
				     struct aesd_seekto *seekto_cmd,
				     char *packet, uint16_t send_buffer_size);
static uint8_t process_packet(int sock_fd, char *packet, uint16_t packet_index,
			      uint16_t packet_size);
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
static uint8_t send_file(int sock_fd, char *buffer, uint32_t buffer_size,
			 long seek_pos)
{
	uint8_t ret_val = EXIT_SUCCESS;
	size_t bytes_read;
	ssize_t bytes_sent;
	size_t total_bytes_sent;

	if (0 != fseek(log_file, seek_pos, SEEK_SET)) {
		syslog(LOG_ERR, "Error seeking to start of file: %s\n",
		       strerror(errno));
		ret_val = EXIT_FAILURE;
	}

	if (EXIT_SUCCESS == ret_val) {
		do {
			bytes_read = fread(buffer, 1, buffer_size, log_file);
			total_bytes_sent = 0U;
			while (bytes_read > total_bytes_sent) {
				bytes_sent =
					send(sock_fd, buffer + total_bytes_sent,
					     bytes_read - total_bytes_sent,
					     MSG_NOSIGNAL);
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

		if (0 != fseek(log_file, 0, SEEK_END)) {
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

static uint8_t convert_str_to_long(const char *str, long *conversion_result,
				   long min_value, long max_value)
{
	uint8_t ret_val = FALSE;
	char *endptr;

	*conversion_result = strtol(str, &endptr, 10);

	if (endptr == str) {
		syslog(LOG_ERR, "Error parsing x_str: %s\n", strerror(errno));
	} else if (*endptr != '\0') {
		syslog(LOG_ERR, "Part of x_str is not a number: %s\n",
		       strerror(errno));
	} else if ((*conversion_result < min_value) ||
		   (*conversion_result > max_value)) {
		syslog(LOG_ERR, "x_str is not a valid number: %s\n",
		       strerror(errno));
	} else {
		ret_val = TRUE;
	}

	return ret_val;
}

static uint8_t parse_x_str_and_y_str(const char *xy_str,
				     struct aesd_seekto *seekto_cmd)
{
	uint8_t ret_val = TRUE;
	long conversion_result;

	char *x_str = strtok((char *)xy_str, ",");
	char *y_str = strtok(NULL, ",");

	if (x_str == NULL || y_str == NULL) {
		ret_val = FALSE;
		syslog(LOG_ERR, "Error parsing x_str or y_str: %s\n",
		       strerror(errno));
	}

	if (ret_val) {
		ret_val = convert_str_to_long(x_str, &conversion_result,
					      (long)0L, (long)UINT32_MAX);
		seekto_cmd->write_cmd = (uint32_t)conversion_result;
	}

	if (ret_val) {
		ret_val = convert_str_to_long(y_str, &conversion_result,
					      (long)0L, (long)UINT32_MAX);
		seekto_cmd->write_cmd_offset = (uint32_t)conversion_result;
	}

	return ret_val;
}

static uint8_t parse_xy_str(const char *packet, uint16_t packet_length,
			    struct aesd_seekto *seekto_cmd)
{
	uint8_t ret_val = TRUE;
	char *xy_str;

	xy_str = malloc(packet_length);
	if (xy_str == NULL) {
		ret_val = FALSE;
		syslog(LOG_ERR, "Error allocating memory for xy_str: %s\n",
		       strerror(errno));
	}

	if (ret_val) {
		/* Keep the original packet string intact in case it is not a
		 * seekto packet */
		strncpy(xy_str, packet, packet_length);
		/* Add null terminator to the end of the string for strtok */
		xy_str[packet_length - 1] = '\0';
		ret_val = parse_x_str_and_y_str(xy_str, seekto_cmd);
	}

	free(xy_str);
	return ret_val;
}

static uint8_t is_seekto_packet(char *packet, uint16_t packet_length,
				struct aesd_seekto *seekto_cmd)
{
	uint8_t ret_val = FALSE;

	if (packet_length > SEEKTO_PACKET_PREFIX_LENGTH) {
		ret_val = strncmp(packet, SEEKTO_PACKET_PREFIX,
				  SEEKTO_PACKET_PREFIX_LENGTH) == 0;
	}

	if (ret_val) {
		/* Remove prefix from the packet */
		ret_val = parse_xy_str(packet + SEEKTO_PACKET_PREFIX_LENGTH,
				       packet_length -
					       SEEKTO_PACKET_PREFIX_LENGTH,
				       seekto_cmd);
	}

	return ret_val;
}

static uint8_t process_seekto_packet(int sock_fd,
				     struct aesd_seekto *seekto_cmd,
				     char *packet, uint16_t send_buffer_size)
{
	uint8_t ret_val = EXIT_SUCCESS;

	pthread_mutex_lock(&log_file_mutex);
#if (USE_AESD_CHAR_DEVICE == 1U)
	open_log_file();
#endif
	int ioctl_ret_val =
		ioctl(fileno(log_file), AESDCHAR_IOCSEEKTO, (void *)seekto_cmd);

	if (ioctl_ret_val < 0) {
		syslog(LOG_ERR, "Error seeking to position: %s\n",
		       strerror(errno));
		ret_val = EXIT_FAILURE;
	}

	if (EXIT_SUCCESS == ret_val) {
		ret_val = send_file(sock_fd, packet, send_buffer_size,
				    (long)ioctl_ret_val);
	}
#if (USE_AESD_CHAR_DEVICE == 1U)
	close_log_file();
#endif
	pthread_mutex_unlock(&log_file_mutex);

	return ret_val;
}

static uint8_t flush_packet_to_client(int sock_fd, char *packet,
				      uint16_t packet_len,
				      uint16_t send_buffer_size)
{
	uint8_t ret_val;

	pthread_mutex_lock(&log_file_mutex);
#if (USE_AESD_CHAR_DEVICE == 1U)
	open_log_file();
#endif
	fwrite(packet, 1, packet_len, log_file);
	fflush(log_file);
	ret_val = send_file(sock_fd, packet, send_buffer_size, 0L);
#if (USE_AESD_CHAR_DEVICE == 1U)
	close_log_file();
#endif
	pthread_mutex_unlock(&log_file_mutex);

	if (EXIT_SUCCESS != ret_val) {
		syslog(LOG_ERR, "Error sending file: %s\n", strerror(errno));
	}

	return ret_val;
}

static uint8_t process_packet(int sock_fd, char *packet, uint16_t packet_index,
			      uint16_t packet_size)
{
	uint8_t ret_val;

#if (USE_AESD_CHAR_DEVICE == 1U)
	struct aesd_seekto seekto_cmd;
	if (is_seekto_packet(packet, packet_index, &seekto_cmd)) {
		ret_val = process_seekto_packet(sock_fd, &seekto_cmd, packet,
						packet_size);
	} else
#endif
	{
		ret_val = flush_packet_to_client(sock_fd, packet, packet_index,
						 packet_size);
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
			ret_val = process_packet(sock_fd, *packet,
						 *packet_index, *packet_size);

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
	pthread_cond_signal(&thread_list_cond);
	pthread_mutex_unlock(&thread_list_mutex);

	pthread_exit(NULL);
}
