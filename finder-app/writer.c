#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

int main(int argc, char *argv[])
{
	openlog(NULL, 0, LOG_USER);

    char *WRITE_FILE_PATH, *WRITE_STR;
    if (argc != 3)
    {
        if (argc == 1)
        {
        	syslog(LOG_ERR, "Write file path and write string not specified\n");
        }
        else if (argc == 2)
        {
        	syslog(LOG_ERR, "Write string not specified\n");
        }
        else
        {
        	syslog(LOG_ERR, "Number of parameters specified greater than needed\n");
        }
        return 1;
    }
    else
    {
        WRITE_FILE_PATH = argv[1];
        WRITE_STR = argv[2];
    }

	syslog(LOG_DEBUG, "Writing %s to file %s\n", WRITE_STR, WRITE_FILE_PATH);
    
    FILE *file = fopen(WRITE_FILE_PATH, "w");
    int rc = fwrite(WRITE_STR, 1, strlen(WRITE_STR), file);
    int retVal = 0;
    if (rc <= 0)
    {
        syslog(LOG_ERR, "Error writing file\n");
        retVal = 1;
    }
    fclose(file);

    return retVal;
}
