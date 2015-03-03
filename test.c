#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char ** argv)
{
	struct interface {
		char * remoteIP;
		int remotePort;
		char * myVIP;
		char * remoteVIP;
		int status;
	};

	FILE * fp;
	char * line;
	char * myIP;
	int myPort;
	size_t len = 0;

	struct interface interfaceArr[16];

	int count = -1;

	fp = fopen(argv[1], "r");
	if (fp == NULL)
		exit(EXIT_FAILURE);

	while (getline(&line, &len, fp) != -1) {
		if(count == -1) {
			char* saveptr;
			myIP = strtok_r(line, ":", &saveptr);
			myPort = atoi(strtok_r(NULL, ":", &saveptr));

			count ++;
		}
		else {
			char *saveptr, *saveptr1;
			char *token;

			token = strtok_r(line, " ", &saveptr);

			struct interface tempInt;
			tempInt.myVIP 		= strtok_r(NULL, " ", &saveptr);
			tempInt.remoteVIP 	= strtok_r(NULL, " ", &saveptr);
			tempInt.remoteIP 	= strtok_r(line, ":", &saveptr1);
			tempInt.remotePort	= atoi(strtop_r(NULL, ":", &saveptr1));

			interfaceArr[count] = tempInt;
			count++;
		}

		// printf("%s", line);
	}

	printf("%s\n", myIP);

	fclose(fp);
	if (line)
		free(line);
	exit(EXIT_SUCCESS);

	// if (argc < 3) {
	// 	printf("Command should be: myprog s <port> or myprog c <port> <address>\n");
	// 	return 1;
	// }
	// int port = atoi(argv[2]);
	// if (port < 1024 || port > 65535) {
	// 	printf("Port number should be equal to or larger than 1024 and smaller than 65535\n");
	// 	return 1;
	// }
	// if (argv[1][0] == 'c') {
	// 	if(argv[3]==NULL){
	// 		printf("NO IP address is given\n");
	// 		return 1;
	// 	}
	// 	return client(argv[3], port);
	// } else if (argv[1][0] == 's') {
	// 	return server(port);
	// } else {
	// 	printf("unknown command type %s\nCommand should be: myprog s <port> or myprog c <port> <address>", argv[1]);
	// 	return 1;
	// }
	// return 0;
}