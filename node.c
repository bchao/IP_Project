#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <netinet/in.h>

void* server();
int client(const char * addr, uint16_t port, char *msg);
void *parse_input();

#define MAX_MSG_LENGTH (1400)
#define BUF_LENGTH (64*1024) // 64 KiB

typedef struct {
	u_char		ip_p;				/* protocol */
	u_char		ip_ttl;				/* time to live */
	u_char		ip_hl:4;				/* header length */
			// ip_v:4;				/* version */
	short		ip_len;				/* total length */
	u_short		ip_id;				/* identification */
	short		ip_off;				/* fragment offset field */
	u_short		ip_sum;				/* checksum */
	uint32_t	ip_src;				/* source address */
	uint32_t	ip_dst;				/* dest address */
} ip;

ip createIPHeader(char * sAddress, char * dAddress, int p, char * msg);

typedef struct {
	char remoteIP[20];
	int remotePort;
	char myVIP[20];
	char remoteVIP[20];
	char status[20];
	int interface_id;
} interface;

typedef struct {
	char dAddress[20];
	int nextHop;
	int cost;
} routeTableEntry;

int interfaceCount;
int rtableCount;
int myPort;
char myIP[20];
interface interfaceArr[16];
routeTableEntry routeTable[16];

int main(int argc, char ** argv)
{
	/* READ IN INPUT FILE */

	FILE * fp;
	char line[121];
	char *item;
	interfaceCount = -1;
	
	fp = fopen(argv[1], "r");

	while (fgets(line, 120, fp)) {
		int i;
		for(i = 0; line[i]!='\0';i++) {
			if(line[i]=='\n')
				line[i]=' ';
		}
		
		if(interfaceCount == -1) {
			char * temp;
			item = strtok_r(line, ":", &temp);
			if(strcmp(item, "localhost") == 0) {
				strcpy(myIP, "127.0.0.1");
			}
			else {
				strcpy(myIP, item);
			}

			item = strtok_r(NULL, ":", &temp);
			myPort = atoi(item);

			interfaceCount++;
		}
		else {
			printf("%s\n", line);
			char * temp;
			item = strtok_r(line, ":", &temp);
			if(strcmp(item, "localhost") == 0) {
				strcpy(interfaceArr[interfaceCount].remoteIP, "127.0.0.1");
			}
			else {
				strcpy(interfaceArr[interfaceCount].remoteIP, item);
			}
			// strcpy(interfaceArr[interfaceCount].remoteIP, item);

			char * token = strtok_r(NULL, ":", &temp);
			item = strtok_r(token, " ", &temp);
			interfaceArr[interfaceCount].remotePort = atoi(item);

			item = strtok_r(NULL, " ", &temp);
			strcpy(interfaceArr[interfaceCount].myVIP, item);

			item = strtok_r(NULL, " ", &temp);
			strcpy(interfaceArr[interfaceCount].remoteVIP, item);

			interfaceArr[interfaceCount].interface_id = (interfaceCount + 1);
			strcpy(interfaceArr[interfaceCount].status, "up");

			interfaceCount++;
		}
	}

	fclose(fp);

	// hardcoded routing table
	rtableCount = 2;
	strcpy(routeTable[0].dAddress, "10.116.89.157");
	routeTable[0].nextHop = 1;
	routeTable[0].cost = 1;
	strcpy(routeTable[1].dAddress, "14.230.5.36");
	routeTable[1].nextHop = 2;
	routeTable[1].cost = 1;


	/* RUN AS UDP SERVER */
	pthread_t server_thread;
	if(pthread_create(&server_thread, NULL, server, NULL)) {
		fprintf(stderr, "Error creating thread\n");
		return 1;
	}

	/* CREATE ANOTHER THREAD TO LOOP AND WAIT FOR USER INPUT */
	
	parse_input();

	/* TRY TO MAKE CALLS TO LINK LAYER */

	return 0;
}

void *parse_input() {
	char uInp[512];
	while (1) {
		fflush(stdin);
		printf("Enter command: \n");
		gets(uInp);

		char *temp;
		char *firstWord = strtok_r(uInp, " ", &temp);

		if(strcmp(firstWord, "ifconfig") == 0) {
			int i;
			for (i = 0; i < interfaceCount; i++) {
				printf("%d %s %s\n", interfaceArr[i].interface_id, interfaceArr[i].myVIP, interfaceArr[i].status);
			}
		}
		else if (strcmp(firstWord, "routes") == 0) {
			printf("routes\n");
		}
		else if (strcmp(firstWord, "down") == 0) {
			int interface_id = atoi(strtok_r(NULL, " ", &temp));
			strcpy(interfaceArr[interface_id - 1].status, "down");
			printf("Interface %d down\n", interface_id);
		}
		else if (strcmp(firstWord, "up") == 0) {
			int interface_id = atoi(strtok_r(NULL, " ", &temp));
			strcpy(interfaceArr[interface_id - 1].status, "up");
			printf("Interface %d up\n", interface_id);
		}
		else if (strcmp(firstWord, "send") == 0) {
			char *VIPaddress = strtok_r(NULL, " ", &temp);
			char *message = strtok_r(NULL, " ", &temp);

			printf("Should send %s to %s\n", message, VIPaddress);

			int i, rem_port;
			char *physAddress;

			// currently getting from interface array
			// need to get things from routing table eventually

			// for (i = 0; i < interfaceCount; i++) {
			// 	if(strcmp(interfaceArr[i].remoteVIP, VIPaddress) == 0) {
			// 		rem_port = interfaceArr[i].remotePort;
			// 		physAddress = interfaceArr[i].remoteIP;
			// 		break;
			// 	}
			// }
			
			for (i = 0; i < rtableCount; i++) {
				if(strcmp(VIPaddress, routeTable[i].dAddress) == 0) {
					int nextHop = routeTable[i].nextHop - 1;
					rem_port = interfaceArr[nextHop].remotePort;
					physAddress = interfaceArr[nextHop].remoteIP;
					break;
				}
			}

			// ip testIP = createIPHeader(myIP, VIPaddress, 0, message);
			// printf("%d %d %d", testIP.ip_src, testIP.ip_dst, testIP.ip_p);

			printf("address: %s\nport: %d\n", physAddress, rem_port);

			// char hard_address[512];
			// strcpy(hard_address, "127.0.0.1");
			// int hard_port = 17001;
			client(physAddress, rem_port, message);
		}
		else {
			printf("not a correct input\n");
		}
	}
	return 0;
}

// typedef struct {
// 	u_char		ip_p;				/* protocol */
// 	u_char		ip_ttl;				/* time to live */
// 	u_char		ip_hl:4				/* header length */
// 			// ip_v:4;				/* version */
// 	short		ip_len;				/* total length */
// 	u_short		ip_id;				/* identification */
// 	short		ip_off;				/* fragment offset field */
// 	u_short		ip_sum;				/* checksum */
// 	uint32_t	ip_src				/* source address */
// 	uint32_t	ip_dst;				/* dest address */
// } ip;

ip createIPHeader(char * sAddress, char * dAddress, int p, char * msg) {
	ip header;
	header.ip_p = p;
	// header.ip_ttl = 16;
	// header.ip_hl = sizeof(header);
	header.ip_src = inet_addr(sAddress);
	printf("sAddress %s -> %d\n", sAddress, header.ip_src);
	header.ip_dst = inet_addr(dAddress);
	printf("dAddress %s -> %d\n", dAddress, header.ip_dst);

	return header;
}

int client(const char * addr, uint16_t port, char msg[])
{
	int sock;
	struct sockaddr_in server_addr;

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Create socket error:");
		return 1;
	}

	printf("Socket created on client\n");
	server_addr.sin_addr.s_addr = inet_addr(addr);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	if (sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("Sending error:");
		return 1;
	}

	close(sock);
	return 0;
}

void *server()
{
	struct sockaddr_in server_addr, client_addr;
	int sock;
	int recvlen;
	socklen_t len = sizeof(client_addr);
	char msg[MAX_MSG_LENGTH];

	bzero((char *)&server_addr, sizeof(server_addr));
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(myPort);

	// Create socket using UDP datagrams
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Create socket error:");
		return (void*) 1;
	}
	printf("Socket created on server port %i.\n", myPort);
	// Bind socket to local address
	if ((bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr))) < 0) {
		perror("Bind socket error:");
		return (void*) 1;
	}

	while(1) {
		if (recvlen = recvfrom(sock, msg, MAX_MSG_LENGTH, 0, (struct sockaddr *)&client_addr, &len) < 0) {
			perror("Receiving error:");
			return (void*) 1;
		}
		printf("%s\n", msg);
		msg[recvlen] = 0;
	}
	close(sock);
	return (void*) 0;
}
