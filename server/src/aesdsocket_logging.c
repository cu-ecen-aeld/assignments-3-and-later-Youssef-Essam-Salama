/*
 * Author: Youssef Essam Salama
 * Date: 2026-08-21
 * Version: 2.0
 * Description: AESD Assignment 6 - Logging functions
 */

/*
   ############################################################
   ################# Include libraries ########################
   ############################################################
*/
#include "aesdsocket_logging.h"
#include "aesdsocket_cfg.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

/*
   ############################################################
   ################# Global variables #########################
   ############################################################
*/
FILE *log_file = NULL;
pthread_mutex_t log_file_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
   ############################################################
   ################# Global functions #########################
   ############################################################
*/
uint8_t open_log_file(void)
{
	uint8_t ret_val = EXIT_SUCCESS;

	log_file = fopen(LOG_FILE_NAME, "w+");
	if (log_file == NULL) {
		syslog(LOG_ERR, "Error opening file: %s\n", strerror(errno));
		ret_val = EXIT_FAILURE;
	}

	return ret_val;
}

void close_log_file(void)
{
	if (log_file != NULL) {
		syslog(LOG_INFO, "Closing log file\n");
		fclose(log_file);
		log_file = NULL;
	}
}
