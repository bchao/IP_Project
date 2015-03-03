#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

//#include "link_layer.h"
int server(uint16_t port);
int client(const char * addr, uint16_t port, char msg[]);
void *parse_input(void* something);

#define MAX_MSG_LENGTH (512)
#define MAX_BACK_LOG (5)

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
	char myIP[20];
	int myPort;

	fp = fopen(argv[1], "r");

	while (fgets(line, 120, fp)) {
		if(count == -1) {
			char * temp;
			item = strtok_r(line, ":", &temp);
			strcpy(myIP, item);

			item = strtok_r(NULL, ":", &temp);
			myPort = atoi(item);

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
	pthread_t server_thread;
	if(pthread_create(&server_thread, NULL, server, (void*) myPort)) {
		fprintf(stderr, "Error creating thread\n");
		return 1;
	}
	// server(myPort);

	/* CREATE ANOTHER THREAD TO LOOP AND WAIT FOR USER INPUT */
	
	// pthread_t user_input_thread;
	// if(pthread_create(&user_input_thread, NULL, parse_input, (void*) interfaceArr)) {
	// 	fprintf(stderr, "Error creating thread\n");
	// 	return 1;
	// }
	parse_input();

	/* TRY TO MAKE CALLS TO LINK LAYER */

	return 0;

	//return getInput();
}

void *parse_input() {
	char msg[512];
	printf("Waiting for user input.\n");
	while (1) {
		fflush(stdin);
		printf("Enter message: \n");
		gets(msg);

		char *temp;
		char *firstWord = strtok_r(msg, " ", &temp);

		if(strcmp(firstWord, "ifconfig") == 0) {
			printf("ifconfig\n");
		}
		else if (strcmp(firstWord, "routes") == 0) {
			printf("routes\n");
		}
		else if (strcmp(firstWord, "down") == 0) {
			int interface_id = atoi(strtok_r(NULL, " ", &temp));
			printf("Interface %d down\n", interface_id);
		}
		else if (strcmp(firstWord, "up") == 0) {
			int interface_id = atoi(strtok_r(NULL, " ", &temp));
			printf("Interface %d up\n", interface_id);
		}
		else if (strcmp(firstWord, "send") == 0) {
			char *address = strtok_r(NULL, " ", &temp);
			printf("%s\n", address);
			char *message = strtok_r(NULL, " ", &temp);

			// hardcoded address and port for now
			char hard_address[512];
			strcpy(hard_address, "127.0.0.1");
			int hard_port = 9999;
			client(hard_address, hard_port, message);

			printf("%s\n", message);
			printf("send %s to %s\n", message, address);
		}
		else {
			printf("not a correct input\n");
		}

	}

	return 0;
}

int client(const char * addr, uint16_t port, char msg[])
{
	int sock;
	struct sockaddr_in server_addr;
	socklen_t len = sizeof(server_addr);

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Create socket error:");
		return 1;
	}

	printf("Socket created\n");
	server_addr.sin_addr.s_addr = inet_addr(addr);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	while (1) { //CHANGE WHILE LOOP TO TAKE IN INPUT FROM FILE
		if (sendto(sock, msg, MAX_MSG_LENGTH, 0, (struct sockaddr *)&server_addr, len) < 0) {
			perror("Sending error:");
			return 1;
		}

	}
	close(sock);
	return 0;
}

int server(uint16_t port)
{
	struct sockaddr_in server_addr, client_addr;
	int sock;
	socklen_t len = sizeof(client_addr);
	char msg[MAX_MSG_LENGTH];

	bzero((char *)&server_addr, sizeof(server_addr));
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	// Create socket using UDP datagrams
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Create socket error:");
		return 1;
	}
	printf("Socket created on port %i.\n", port);
	// Bind socket to local address
	if ((bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr))) < 0) {
		perror("Bind socket error:");
		return 1;
	}

	while(1) {
		if (recvfrom(sock, msg, MAX_MSG_LENGTH, 0, (struct sockaddr *)&client_addr, &len) < 0) {
			perror("Receiving error:");
			return 1;
		}
		printf("Received packet from %s:%d\nData: %s\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), msg);
	}
	close(sock);
	return 0;
}
