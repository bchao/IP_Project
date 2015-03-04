#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <netinet/in.h>
#include <inttypes.h>

#define MAX_MSG_LENGTH (1400) // max length of a packet to be sent
#define BUF_LENGTH (64*1024) // 64 KiB
#define HEADER_SIZE (18) // total size of our IP header
#define EVICTION_TIME (12) // 12 seconds to evict

typedef struct {
	u_char		ip_p;				/* protocol */
	u_char		ip_ttl;				/* time to live */
	short		ip_len;				/* total length */
	short		ip_off;				/* fragment offset field */
	int			ip_sum;				/* checksum */
	uint32_t	ip_src;				/* source address */
	uint32_t	ip_dst;				/* destination address */
	char payload[1400 - HEADER_SIZE];
} ip;

typedef struct {
	uint16_t command;
	uint16_t num_entries;
	struct {
		uint32_t cost;
		uint32_t address;
	} entries[64];
} RIP;

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
	time_t last_updated;
} routeTableEntry;

char *serialize(ip * ipStruct, char* buf);
ip deserialize(char * buf);
ip createIPPacket(char * sAddress, char * dAddress, int p, char * msg);
ip createRIPPacket(char * sAddress, char * dAddress, int p, RIP * rip);
void * updateRoutingTable(char * payload, uint32_t ip_dest);
void *server();
void *send_initial_requests();
void *send_updates();
void *evict_entries();
int client(const char * addr, uint16_t port, char *msg);
void *parse_input();
int ip_sum(char* packet, int len);

int interfaceCount;
int rTableCount;
int myPort;
char myIP[20];
interface interfaceArr[16];
routeTableEntry routeTable[64];

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

	int i;
	// initialize routing table
	for(i = 0; i < interfaceCount; i++) {
		strcpy(routeTable[i].dAddress, interfaceArr[i].remoteVIP);
		routeTable[i].nextHop = interfaceArr[i].interface_id;
		routeTable[i].cost = 1;

		rTableCount++;
	}

	/* NEW THREAD TO RUN AS UDP SERVER */
	pthread_t server_thread;
	if(pthread_create(&server_thread, NULL, server, NULL)) {
		fprintf(stderr, "Error creating server thread\n");
		return 1;
	}
	printf("A\n");

	send_initial_requests();

	printf("B\n");

	// /* NEW THREAD TO LOOP AND SEND OUT UPDATE RIP PACKETS */
	pthread_t update_thread;
	if(pthread_create(&update_thread, NULL, send_updates, NULL)) {
		fprintf(stderr, "Error creating update thread\n");
		return 1;
	}

	printf("C\n");

	// /* NEW THREAD TO LOOP AND EVICT */
	pthread_t evict_thread;
	if(pthread_create(&evict_thread, NULL, evict_entries, NULL)) {
		fprintf(stderr, "Error creating update thread\n");
		return 1;
	}

	printf("D\n");


	/* LOOP AND WAIT FOR USER INPUT */
	
	parse_input();

	return 0;
}

/*  Send initial routing requests to all of the interfaces given in the input file
 */
void *send_initial_requests() {
	int i;
	for (i = 0; i < interfaceCount; i++) {
		// BUILD RIP PACKET
		RIP message;
		message.command = 1; // SHOULD THIS BE A REQUEST OR RESPONSE (1 or 2)?!?
		message.num_entries = 0;

		ip request_packet = createRIPPacket(myIP, interfaceArr[i].remoteVIP, 200, &message);
		char serialized[MAX_MSG_LENGTH];			
		serialize(&request_packet, serialized);

		client(interfaceArr[i].remoteIP, interfaceArr[i].remotePort, serialized);				
	}
}

/* Loops infinitely over the routing entries
   If any of the entries haven't been updated in the last 12 seconds, set its cost to infinity (16)
*/
void *evict_entries() {
	int i;
	while(1) {
		for(i = 0; i < rTableCount; i++) {
			if(time(0) - routeTable[i].last_updated > EVICTION_TIME) {
				routeTable[i].cost = 16;
			}
		}
	}
}

/* Loops infinitely and sends out updates to all interfaces after sleeping every 5 seconds
   Build the RIP packet from routing table and send to each interface
*/
void *send_updates() {
	int i;
	while(1) {
		for (i = 0; i < interfaceCount; i++) {
			// Check if interface is down, if so don't update
			if (strcmp(interfaceArr[i].status, "down") == 0 ) { 
				continue; 
			}

			// BUILD RIP PACKET
			RIP message;
			message.command = 2;
			message.num_entries = rTableCount;

			int j;
			for(j = 0; j < rTableCount; j++) {
				// SPECIAL POISON REVERSE CONDITION? SET THE COST TO infinity (16)
				if(routeTable[j].nextHop == interfaceArr[i].interface_id) {
					(message.entries)[j].cost = 16;
				}
				else {
					(message.entries)[j].cost = routeTable[j].cost;
				}	
				(message.entries)[j].address = inet_addr(routeTable[j].dAddress);	
			}

			ip update_packet = createRIPPacket(myIP, interfaceArr[i].remoteVIP, 200, &message);
			char serialized[MAX_MSG_LENGTH];			
			serialize(&update_packet, serialized);

			client(interfaceArr[i].remoteIP, interfaceArr[i].remotePort, serialized);				
		}
		sleep(5);
	}
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

			//find route in routeTable and update distance to inf
			int i;
			for(i = 0; i < rTableCount; i++) {
				if(routeTable[i].nextHop == interface_id) {
					routeTable[i].cost = 16;
					break;
				}
			}

			printf("Interface %d down with cost\n", interface_id);
		}
		else if (strcmp(firstWord, "up") == 0) {
			int interface_id = atoi(strtok_r(NULL, " ", &temp));
			strcpy(interfaceArr[interface_id - 1].status, "up");

			//find route in routeTable and update distance to 1
			int i;
			for(i = 0; i < rTableCount; i++) {
				if(routeTable[i].nextHop == interface_id) {
					routeTable[i].cost = 1;
					break;
				}
			}

			printf("Interface %d up\n", interface_id);
		}
		else if (strcmp(firstWord, "send") == 0) {
			char *VIPaddress = strtok_r(NULL, " ", &temp);
		
			char message[MAX_MSG_LENGTH];
			strcpy(message, temp);
			memset(temp, 0, strlen(temp));

			printf("Should send %s to %s\n", message, VIPaddress);

			int i, rem_port;
			char *physAddress;
			
			for (i = 0; i < rTableCount; i++) {
				if(strcmp(VIPaddress, routeTable[i].dAddress) == 0) {
					int nextHop = routeTable[i].nextHop - 1;
					rem_port = interfaceArr[nextHop].remotePort;
					physAddress = interfaceArr[nextHop].remoteIP;
					break;
				}
			}

			ip testIP = createIPPacket(myIP, VIPaddress, 0, message);

			// RIP rip;
			// rip.command = 10;
			// rip.num_entries = 2;
			// rip.entries[0].cost = 1;
			// rip.entries[0].address = 100;
			// rip.entries[1].cost = 2;
			// rip.entries[1].address = 200;
			// ip testIP = createRIPPacket(myIP, VIPaddress, 200, &rip);
			// printf("%d %d %d", testIP.ip_src, testIP.ip_dst, testIP.ip_p);

			char serialized[MAX_MSG_LENGTH];
			
			serialize(&testIP, serialized);

			// ip deserialized = deserialize(serialized);

			// printf("protocol %d\n", deserialized.ip_p);
			// printf("ttl %d\n", deserialized.ip_ttl);
			// printf("source %d\n", deserialized.ip_src);
			// printf("dest %d\n", deserialized.ip_dst);

			// printf("serialized thing before socket %i\n", (int) strlen(serialized));
			client(physAddress, rem_port, serialized);
			memset(message, 0, MAX_MSG_LENGTH);
		}
		else {
			printf("Incorrect input\n");
		}
	}
	return 0;
}

char * serialize(ip * ipStruct, char* buf) {
	int offset;
	//char * startbuf;
	offset = 0;

	memcpy(buf, &ipStruct->ip_p, sizeof(u_char));
	offset+=sizeof(u_char);
	memcpy(buf + offset, &ipStruct->ip_ttl, sizeof(u_char));
	offset+=sizeof(u_char);
	memcpy(buf + offset, &ipStruct->ip_len, sizeof(short));
	offset+=sizeof(short);
	memcpy(buf + offset, &ipStruct->ip_off, sizeof(short));
	offset+=sizeof(short);
	memcpy(buf + offset, &ipStruct->ip_sum, sizeof(int));
	offset+=sizeof(int);
	memcpy(buf + offset, &ipStruct->ip_src, sizeof(uint32_t));
	offset+=sizeof(uint32_t);
	memcpy(buf + offset, &ipStruct->ip_dst, sizeof(uint32_t));
	offset+=sizeof(uint32_t);

	printf("protocol = %d\n", ipStruct->ip_p);

	memcpy(buf + offset, &ipStruct->payload, (1400 - HEADER_SIZE));
	offset+=(1400-HEADER_SIZE);

	buf[MAX_MSG_LENGTH] = '\0';
	// int i;
	// for (i = 0; i < 40; i++) {
	// 	printf("Character at %i: %c\n", i, buf[i]);
	// }
	// printf("length: %d\n", (int) strlen(buf));

	return buf;
}

ip deserialize(char * buf) {
	ip ipStruct;
	//char * startbuf;
	int offset = 0;

	memcpy(&ipStruct.ip_p, buf, sizeof(u_char));
	offset+=sizeof(u_char);
	memcpy(&ipStruct.ip_ttl, buf + offset, sizeof(u_char));
	offset+=sizeof(u_char);
	memcpy(&ipStruct.ip_len, buf + offset, sizeof(short));
	offset+=sizeof(short);
	memcpy(&ipStruct.ip_off, buf + offset, sizeof(short));
	offset+=sizeof(short);
	memcpy(&ipStruct.ip_sum, buf + offset, sizeof(int));
	offset+=sizeof(int);
	memcpy(&ipStruct.ip_src, buf + offset, sizeof(uint32_t));
	offset+=sizeof(uint32_t);
	memcpy(&ipStruct.ip_dst, buf + offset, sizeof(uint32_t));
	offset+=sizeof(uint32_t);

	// printf("protocol: %d\n", ipStruct.ip_p);

	memcpy(&ipStruct.payload, buf + offset, 1400 - HEADER_SIZE);
	offset+=(1400 - HEADER_SIZE);

	return ipStruct;
}

ip createIPPacket(char * sAddress, char * dAddress, int p, char * msg) {
	ip packet;
	packet.ip_p = (int) p;
	packet.ip_ttl = 16;
	packet.ip_len = sizeof(msg);
	packet.ip_off = 0;
	packet.ip_src = inet_addr(sAddress);
	packet.ip_dst = inet_addr(dAddress);
	strcpy(packet.payload, msg);

	// Calculate checksum
	int checksum = ip_sum((char*) &packet, MAX_MSG_LENGTH);
	packet.ip_sum = checksum;

	return packet;
}

ip createRIPPacket(char * sAddress, char * dAddress, int p, RIP * rip) {
	ip packet;
	int n_entries, i;

	packet.ip_p = (int) p;
	packet.ip_ttl = 16;
	packet.ip_len = sizeof(rip);
	packet.ip_off = 0;
	packet.ip_src = inet_addr(sAddress);
	packet.ip_dst = inet_addr(dAddress);

	int offset = 0;
	memcpy(packet.payload + offset, &rip->command, sizeof(uint16_t));
	offset+=sizeof(uint16_t);
	memcpy(packet.payload + offset, &rip->num_entries, sizeof(uint16_t));
	offset+=sizeof(uint16_t);
	n_entries = (int) rip->num_entries;

	for(i = 0; i < n_entries; i++) {
		memcpy(packet.payload + offset, &rip->entries[i].cost, sizeof(uint32_t));
		offset+=sizeof(uint32_t);
		memcpy(packet.payload + offset, &rip->entries[i].address, sizeof(uint32_t));
		offset+=sizeof(uint32_t);
	}
	// Calculate checksum
	int checksum = ip_sum((char*) &packet, MAX_MSG_LENGTH);
	packet.ip_sum = checksum;

	return packet;
}

int client(const char * addr, uint16_t port, char msg[])
{
	int sock;
	struct sockaddr_in server_addr;

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Create socket error:");
		return 1;
	}

	//printf("serialized thing in socket %i\n", (int) strlen(msg));
	printf("Socket created on client\n");
	server_addr.sin_addr.s_addr = inet_addr(addr);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	if (sendto(sock, msg, MAX_MSG_LENGTH, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
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
		// 	printf("serialized thing on recv serv %i\n", (int) strlen(msg));
		// temporarily assume always going to forward protocol 0
		// not worry about TTL and checksum
		// check if it's at destination
		// if at destination, print. If not, forward

		ip deserialized = deserialize(msg);

		printf("protocol %d\n", deserialized.ip_p);
		printf("ttl %d\n", deserialized.ip_ttl);
		printf("source %d\n", deserialized.ip_src);
		printf("dest %d\n", deserialized.ip_dst);


		int checksum = deserialized.ip_sum;
		deserialized.ip_sum = 0;
		if (ip_sum((char*) &deserialized, MAX_MSG_LENGTH) != checksum) {
			printf("Checksums different, message dropped. \n");
			return;
		}

		if(deserialized.ip_ttl == 0) {
			printf("Message dropped.\n");
			return;
		} else {
			deserialized.ip_ttl--;
		}

		if(deserialized.ip_p == 0) {
			//check if it's at destination
			int i;
			for (i = 0; i < interfaceCount; i++) {
				// we are at destination, print the message
				if(deserialized.ip_dst == inet_addr(interfaceArr[i].myVIP)) {
					printf("msg %s\n", deserialized.payload);
					break;
				} else { // Check where to forward it to
					int j, rem_port;
					char *physAddress;
					for (j = 0; j < rTableCount; j++) {
						if (deserialized.ip_dst == inet_addr(routeTable[j].dAddress)) {

							// DO I NEED TO UPDATE deserialized.ip_src to myIP here??? NO?

							// Find physical address and rem_port
							int nextHop = routeTable[j].nextHop - 1;
							rem_port = interfaceArr[nextHop].remotePort;
							physAddress = interfaceArr[nextHop].remoteIP;

							// Recalculate checksum before forwarding
							deserialized.ip_sum = ip_sum((char*) &deserialized, MAX_MSG_LENGTH);

							char serialized[MAX_MSG_LENGTH];	
							serialize(&deserialized, serialized);
							// Forward to next place
							client(physAddress, rem_port, serialized);
						}
					}
				}
			}


		
		} else if(deserialized.ip_p == 200) {
			printf("protocol 200\n");
			updateRoutingTable(deserialized.payload, deserialized.ip_dst);
		}

		memset(msg, 0, MAX_MSG_LENGTH);
	}
	close(sock);
	return (void*) 0;
}

void * updateRoutingTable(char * payload, uint32_t ip_dest) {
	int offset = 0;
	int n_entries, i;

	RIP rip;

	memcpy(&rip.command, payload + offset, sizeof(uint16_t));
	offset+=sizeof(uint16_t);
	memcpy(&rip.num_entries, payload + offset, sizeof(uint16_t));
	offset+=sizeof(uint16_t);
	n_entries = (int) rip.num_entries;

	for(i = 0; i < n_entries; i++) {
		memcpy(&rip.entries[i].cost, payload + offset, sizeof(uint32_t));
		offset+=sizeof(uint32_t);
		memcpy(&rip.entries[i].address, payload + offset, sizeof(uint32_t));
		offset+=sizeof(uint32_t);
	}

	printf("command: %d\n", rip.command);
	printf("num_entries: %d\n", rip.num_entries);
	printf("entry 1 cost: %d\n", rip.entries[0].cost);
	printf("entry 2 cost: %d\n", rip.entries[1].cost);

	//now rip is populated

	if(rip.command == 1) {
		// Add new entry to routing table
		int i;
		for(i = 0; i < interfaceCount; i++) {
			if(inet_addr(interfaceArr[i].myVIP) == ip_dest) {
				strcpy(routeTable[rTableCount].dAddress, interfaceArr[i].remoteVIP);
				routeTable[rTableCount].nextHop = interfaceArr[i].interface_id;
			}
		}
		routeTable[rTableCount].cost = 1;

		// Increment routing table count
		rTableCount++;

		// CREATE AND SEND UPDATE MESSAGE back
		RIP response;
			response.command = 2; // SHOULD THIS BE A REQUEST OR RESPONSE (1 or 2)?!?
			response.num_entries = rTableCount;

			int j;
			for(j = 0; j < rTableCount; j++) {
				// SPECIAL POISON REVERSE CONDITION? SET THE COST TO infinity (16)
				if(routeTable[j].nextHop == interfaceArr[i].interface_id) {
					(response.entries)[j].cost = 16;
				}
				else {
					(response.entries)[j].cost = routeTable[j].cost;
				}	
				(response.entries)[j].address = inet_addr(routeTable[j].dAddress);	
			}

			ip update_packet = createRIPPacket(myIP, interfaceArr[i].remoteVIP, 200, &response);
			char serialized[MAX_MSG_LENGTH];			
			serialize(&update_packet, serialized);

			client(interfaceArr[i].remoteIP, interfaceArr[i].remotePort, serialized);
	}
	else if(rip.command == 2) {
		int i, j;
		for(i = 0; i < rip.num_entries; i++) {
			for(j = 0; j < rTableCount; j++) {
				if(inet_addr(routeTable[j].dAddress) == rip.entries[i].address) {
					// update our routing table as needed
					// if current cost is greater than (newcost + 1), then update

					if(routeTable[j].cost > rip.entries[i].cost + 1) {
						routeTable[j].cost = rip.entries[i].cost + 1;
					}

					// update the update_time for each entry
					routeTable[j].last_updated = time(0);
				}
			}
		}
	}

	return NULL;
}

int ip_sum(char* packet, int n) {
  uint16_t *p = (uint16_t*)packet;
  uint16_t answer;
  long sum = 0;
  uint16_t odd_byte = 0;

  while (n > 1) {
    sum += *p++;
    n -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (n == 1) {
    *(uint8_t*)(&odd_byte) = *(uint8_t*)p;
    sum += odd_byte;
  }

  sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
  sum += (sum >> 16);           /* add carry */
  answer = ~sum;                /* ones-complement, truncate*/
  return answer;
}
