/*
 * Author: Youssef Essam Salama
 * Date: 2026-08-21
 * Version: 2.0
 * Description: AESD Assignment 6 - Listening socket create/bind/listen
 *              and close_socket helper
 */

/*
   ############################################################
   ################# Include libraries ########################
   ############################################################
*/
#include "aesdsocket_socket.h"
#include "aesdsocket_common.h"

#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

/*
   ############################################################
   ############## Local functions declarations ################
   ############################################################
*/
static uint8_t create_server_socket(const struct addrinfo *addrinfo);
static uint8_t bind_server_socket(const struct addrinfo *addrinfo);
static uint8_t listen_on_server_socket(void);

/*
   ############################################################
   ################# Global variables #########################
   ############################################################
*/
int server_sock_fd = -1;

/*
   ############################################################
   ################# Local functions definitions ##############
   ############################################################
*/
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

/*
   ############################################################
   ################# Global functions #########################
   ############################################################
*/
void close_socket(int *sock_fd)
{
	shutdown(*sock_fd, SHUT_RDWR);
	close(*sock_fd);
	*sock_fd = -1;
}

uint8_t setup_socket(void)
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
