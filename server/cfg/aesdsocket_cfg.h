#ifndef AESDSOCKET_CFG_H
#define AESDSOCKET_CFG_H

/*
   ############################################################
   ###################### Macros ##############################
   ############################################################
*/
#define USE_AESD_CHAR_DEVICE (1U)

#define SOCKET_PORT "9000"
#define SOCKET_BACKLOG (10U)
#define SOCKET_BUFFER_SIZE (1024U)
#define MAX_PACKET_SIZE (32762U)

#if (USE_AESD_CHAR_DEVICE == 1U)
#define LOG_FILE_NAME "/dev/aesdchar"
#else
#define LOG_FILE_NAME "/var/tmp/aesdsocketdata"
#endif

#define SEEKTO_PACKET_PREFIX "AESDCHAR_IOCSEEKTO:"
#define SEEKTO_PACKET_PREFIX_LENGTH (sizeof(SEEKTO_PACKET_PREFIX) - 1U)

#endif /* AESDSOCKET_CFG_H */
