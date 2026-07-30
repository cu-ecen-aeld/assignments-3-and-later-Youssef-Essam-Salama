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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>

/* 
   ############################################################
   ###################### Macros ##############################
   ############################################################ 
*/
#define FALSE               (0U)
#define TRUE                (1U)
#define SOCKET_PORT         "9000"
#define SOCKET_BACKLOG      (10U)
#define SOCKET_BUFFER_SIZE  (1500U)
#define INITIAL_PACKET_SIZE (4096U)
#define MAX_PACKET_SIZE     (65536U)
#define LOG_FILE_NAME       "/var/tmp/aesdsocketdata"

/* 
   ############################################################
   ################# Local variables ##########################
   ############################################################ 
*/
static FILE *logFile = NULL;
static int sockfd = -1;
static int accepted_sockfd = -1;
static volatile sig_atomic_t thread_running = TRUE;

/* 
   ############################################################
   ############## Local functions declarations ################
   ############################################################ 
*/
static void garbage_collection(void);
static void sigIntTermHandler(int signum);
static void setup_signal_handlers(void);
static void setup_socket(void);
static void setup_log_file(void);
static void send_file(char *buffer, uint32_t buffer_size);
static void handle_recv_send(void);
static void handle_client_connection(void);

/* 
   ############################################################
   ################# Local functions definitions ##############
   ############################################################ 
*/
static void garbage_collection(void)
{
    if (sockfd != -1)
    {
        syslog(LOG_INFO, "Closing socket\n");
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        sockfd = -1;
    }

    if (accepted_sockfd != -1)
    {
        syslog(LOG_INFO, "Closing accepted socket\n");
        shutdown(accepted_sockfd, SHUT_RDWR);
        close(accepted_sockfd);
        accepted_sockfd = -1;
    }

    if (logFile != NULL)
    {
        syslog(LOG_INFO, "Closing log file\n");
        fclose(logFile);
        logFile = NULL;
        syslog(LOG_INFO, "Deleting log file\n");
        unlink(LOG_FILE_NAME);
    }
}

static void sigIntTermHandler(int signum)
{
    thread_running = FALSE;
}

static void setup_signal_handlers(void)
{
    struct sigaction sigIntTermAction;
    sigIntTermAction.sa_handler = &sigIntTermHandler;
    sigemptyset(&sigIntTermAction.sa_mask);
    sigIntTermAction.sa_flags = 0;

    if (0 != sigaction(SIGINT, &sigIntTermAction, NULL))
    {
        syslog(LOG_ERR, "Error setting up SIGINT handler: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (0 != sigaction(SIGTERM, &sigIntTermAction, NULL))
    {
        syslog(LOG_ERR, "Error setting up SIGTERM handler: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static void setup_socket(void)
{
    int socket_reuse_option = 1;
    struct addrinfo *addrinfo;
    struct addrinfo hints;

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    if (0 != getaddrinfo(NULL, SOCKET_PORT, &hints, &addrinfo))
    {
        syslog(LOG_ERR, "Error getting address info: %s\n", gai_strerror(errno));
        exit(EXIT_FAILURE);
    }

    sockfd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
    if (sockfd == -1)
    {
        syslog(LOG_ERR, "Error creating socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (0 != setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &socket_reuse_option, sizeof(socket_reuse_option)))
    {
        syslog(LOG_ERR, "Error setting socket options: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (0 != bind(sockfd, addrinfo->ai_addr, addrinfo->ai_addrlen))
    {
        syslog(LOG_ERR, "Error binding socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(addrinfo);

    if (0 != listen(sockfd, SOCKET_BACKLOG))
    {
        syslog(LOG_ERR, "Error listening on socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);   
    }
}

static void setup_log_file(void)
{
    logFile = fopen(LOG_FILE_NAME, "w+");
    if (logFile == NULL)
    {
        syslog(LOG_ERR, "Error opening file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static void send_file(char *buffer, uint32_t buffer_size)
{
    size_t bytes_read;
    ssize_t bytes_sent;
    size_t total_bytes_sent;

    if (0 != fseek(logFile, 0, SEEK_SET))
    {
        syslog(LOG_ERR, "Error seeking to start of file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    do {
        bytes_read = fread(buffer, 1, buffer_size, logFile);
        total_bytes_sent = 0U;
        while (bytes_read > total_bytes_sent)
        {
            bytes_sent = send(accepted_sockfd, buffer + total_bytes_sent, bytes_read - total_bytes_sent, 0);
            if (bytes_sent == -1)
            {
                syslog(LOG_ERR, "Error sending data: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
            total_bytes_sent += bytes_sent;
        }
    }while (bytes_read > 0);

    if (0 != fseek(logFile, 0, SEEK_END))
    {
        syslog(LOG_ERR, "Error seeking to end of file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static void handle_recv_send(void)
{
    char *packet;
    char *buffer;
    int bytes_received = 0;
    uint32_t packet_index = 0U;
    uint32_t packet_size = INITIAL_PACKET_SIZE;

    packet = malloc(INITIAL_PACKET_SIZE);
    buffer = malloc(SOCKET_BUFFER_SIZE);
    if (packet == NULL || buffer == NULL)
    {
        syslog(LOG_ERR, "Error allocating memory for packet or buffer: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    do {
        bytes_received = recv(accepted_sockfd, buffer, SOCKET_BUFFER_SIZE, 0);
        if (bytes_received > 0)
        {
            for (uint16_t i = 0U; i < bytes_received; i++)
            {
                if ((packet_index >= packet_size) && (packet_index < MAX_PACKET_SIZE))
                {
                    packet = realloc(packet, packet_size*2);
                    if (packet == NULL)
                    {
                        syslog(LOG_ERR, "Error reallocating memory for packet: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                    }
                    packet_size *= 2;
                }
                else if (packet_index >= MAX_PACKET_SIZE)
                {
                    syslog(LOG_ERR, "Packet size exceeded max size\n");
                    exit(EXIT_FAILURE);
                }
                else
                {
                    /* Do nothing */
                }
                packet[packet_index++] = buffer[i];
                if (buffer[i] == '\n')
                {
                    fwrite(packet, 1, packet_index, logFile);
                    fflush(logFile);
                    send_file(packet, packet_size);
                    packet_index = 0U;
                }
            }
        }
    }
    while (bytes_received > 0);

    free(packet);
    free(buffer);

    if (bytes_received == -1)
    {
        if (TRUE == thread_running)
        {
            syslog(LOG_ERR, "Error receiving data: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
}

static void handle_client_connection(void)
{
    struct sockaddr client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    while (TRUE == thread_running)
    {
        accepted_sockfd = accept(sockfd, &client_addr, &client_addr_len);
        if (accepted_sockfd == -1)
        {
            if (FALSE == thread_running)
            {
                break; /* SIGINT/SIGTERM caught while waiting for connection */
            }
            else
            {
                syslog(LOG_ERR, "Error accepting connection: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            syslog(LOG_INFO, "Accepted connection from %s\n", client_addr.sa_data);
        }

        handle_recv_send();

        shutdown(accepted_sockfd, SHUT_RDWR);
        close(accepted_sockfd);
        accepted_sockfd = -1;
        syslog(LOG_INFO, "Closed connection from %s\n", client_addr.sa_data);
    }
}

/* 
   ############################################################
   ################# Global functions #########################
   ############################################################ 
*/
int main(int argc, char *argv[])
{
    openlog(NULL, 0, LOG_USER);
    setup_signal_handlers();
    atexit(garbage_collection);
    setup_socket();
    setup_log_file();
    handle_client_connection();
    syslog(LOG_INFO, "Caught signal, exiting\n");
    exit(EXIT_SUCCESS);
}
