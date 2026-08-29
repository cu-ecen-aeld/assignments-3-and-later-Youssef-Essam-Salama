#ifndef AESDSOCKET_LOGGING_H
#define AESDSOCKET_LOGGING_H

/*
   ############################################################
   ################# Include libraries ########################
   ############################################################
*/
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

/*
   ############################################################
   ################# Global variables #########################
   ############################################################
*/
extern FILE *log_file;
extern pthread_mutex_t log_file_mutex;

/*
   ############################################################
   ################# Global functions #########################
   ############################################################
*/
uint8_t open_log_file(void);
void close_log_file(void);

#endif /* AESDSOCKET_LOGGING_H */
