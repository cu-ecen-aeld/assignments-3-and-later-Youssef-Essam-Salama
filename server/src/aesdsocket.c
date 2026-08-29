/*
 * Author: Youssef Essam Salama
 * Date: 2026-08-21
 * Version: 2.0
 * Description: AESD Assignment 6 - Server entry point (main, daemon,
 *              signals, accept loop, shared process globals)
 */

/*
   ############################################################
   ################# Include libraries ########################
   ############################################################
*/
#include "aesdsocket.h"
#include "aesdsocket_common.h"
#include "aesdsocket_socket.h"
#include "aesdsocket_threads.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

/*
   ############################################################
   ################# Global variables #########################
   ############################################################
*/
FILE *log_file = NULL;
volatile sig_atomic_t process_running = TRUE;
pthread_mutex_t log_file_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
   ############################################################
   ############## Local functions declarations ################
   ############################################################
*/
static void stop_server_process(void);
static uint8_t handle_input_parameters(int argc, char *argv[]);
static void sig_int_term_handler(int signum);
static uint8_t setup_log_file(void);
static uint8_t setup_signal_handlers(void);
static uint8_t handle_client_connections(void);
static uint8_t setup_daemon(void);
static uint8_t handle_run_as_daemon(void);
static uint8_t run_server(void);
static void free_resources(void);

/*
   ############################################################
   ################# Local variables ##########################
   ############################################################
*/
static uint8_t run_as_daemon = FALSE;

/*
   ############################################################
   ################# Local functions definitions ##############
   ############################################################
*/
static void stop_server_process(void) { process_running = FALSE; }

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

static void sig_int_term_handler(int signum)
{
	(void)signum;
	stop_server_process();
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

	stop_server_process();
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

#if (USE_AESD_CHAR_DEVICE == 0U)
static uint8_t run_time_stamping_thread(pthread_t *time_stamping_thread_id)
{
	uint8_t ret_val = EXIT_SUCCESS;

	ret_val = setup_time_stamping_thread(time_stamping_thread_id);

	if (EXIT_SUCCESS != ret_val) {
		stop_server_process();
		ret_val = EXIT_FAILURE;
	}

	return ret_val;
}
#endif

static uint8_t run_thread_handler(pthread_t *thread_handler_id)
{
	uint8_t ret_val = EXIT_SUCCESS;

	ret_val = setup_thread_handler(thread_handler_id);
	if (EXIT_SUCCESS != ret_val) {
		stop_server_process();
		ret_val = EXIT_FAILURE;
	}

	return ret_val;
}

static uint8_t run_server(void)
{
	uint8_t ret_val = EXIT_SUCCESS;

	uint8_t thread_handler_setup_ret_val = EXIT_FAILURE;
	pthread_t thread_handler_id;

#if (USE_AESD_CHAR_DEVICE == 0U)
	uint8_t time_stamping_thread_setup_ret_val = EXIT_FAILURE;
	pthread_t time_stamping_thread_id;
	time_stamping_thread_setup_ret_val =
		run_time_stamping_thread(&time_stamping_thread_id);
	ret_val = time_stamping_thread_setup_ret_val;
#endif

	if (ret_val == EXIT_SUCCESS) {
		thread_handler_setup_ret_val =
			run_thread_handler(&thread_handler_id);
		ret_val = thread_handler_setup_ret_val;
	}

	if (EXIT_SUCCESS == ret_val) {
		ret_val = handle_client_connections();
	}

	if (EXIT_SUCCESS == thread_handler_setup_ret_val) {
		wake_thread_handler();
		pthread_join(thread_handler_id, NULL);
	}

#if (USE_AESD_CHAR_DEVICE == 0U)
	if (EXIT_SUCCESS == time_stamping_thread_setup_ret_val) {
		pthread_join(time_stamping_thread_id, NULL);
	}
#endif

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
#if (USE_AESD_CHAR_DEVICE == 0U)
		syslog(LOG_INFO, "Deleting log file\n");
		unlink(LOG_FILE_NAME);
#endif
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
