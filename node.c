#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#include "link_layer.h"

int main(int argc, char ** argv)
{
	/* READ IN INPUT FILE */
	typedef struct {
		char remoteIP[20];
		int remotePort;
		char myVIP[20];
		char remoteVIP[20];
		int status;
	} interface;

	interface interfaceArr[16];
	FILE * fp;
	char line[121];
	char *item;
	int count = -1;
	char * myIP;
	int myPort;

	fp = fopen(argv[1], "r");

	while (fgets(line, 120, fp)) {
		if(count == -1) {
			item = strtok(line, ":");
			printf("%s ",item);

			item = strtok(NULL, ":");
			printf("%s ",item);
			printf("\n");
			count++;
		}
		else {
			printf("%s\n", line);
			char * temp;
			item = strtok_r(line, ":", &temp);
			strcpy(interfaceArr[count].remoteIP, item);

			char * token = strtok_r(NULL, ":", &temp);
			item = strtok_r(token, " ", &temp);
			interfaceArr[count].remotePort = atoi(item);

			item = strtok_r(NULL, " ", &temp);
			strcpy(interfaceArr[count].myVIP, item);

			item = strtok_r(NULL, " ", &temp);
			strcpy(interfaceArr[count].remoteVIP, item);
			count++;
		}
	}

	fclose(fp);

	/* RUN AS UDP SERVER */
	server(myPort);

	/* CREATE ANOTHER THREAD TO LOOP AND WAIT FOR USER INPUT */
	p_thread user_input_thread;

	if(pthread_create(&user_input_thread, NULL, parse_input)) {
		fprintf(stderr, "Error creating thread\n");
		return 1;
	}

	/* TRY TO MAKE CALLS TO LINK LAYER */

	return 0;

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

