#include "link_layer.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX_MSG_LENGTH (1400) // MUST SUBTRACT OFF SIZE OF HEADER
#define BUF_LENGTH (64*1024) // 64 KiB

int client(const char * addr, uint16_t port)
{
	int sock;
	struct sockaddr_in client_addr;
	int len = sizeof(client_addr);
	char msg[MAX_MSG_LENGTH];

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Create socket error:");
		return 1;
	}

	printf("Socket created\n");
	client_addr.sin_addr.s_addr = inet_addr(addr);
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(port);

	while (1) { //CHANGE WHILE LOOP TO TAKE IN INPUT FROM FILE
		if (sendto(s, buf, BUF_LEN, 0, &client_addr, slen) < 0) {
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
	int len = sizeof(client_addr);
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
	// Bind socket to local address
	if ((bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr))) < 0) {
		perror("Bind socket error:");
		return 1;
	}

	while(1) {
		if (recvfrom(sock, msg, MAX_MSG_LENGTH, 0, &client_addr, &len) < 0) {
			perror("Receiving error:");
			return 1;
		}
		printf("Received packet from %s:%d\nData: %s\n\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buf);
	}

	close(sock);
	return 0;
}