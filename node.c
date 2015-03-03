#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>

int getInput();

int main(int argc, char ** argv)
{
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

	return getInput();
}

int getInput() {
	char msg[512];

	while (1) {
		fflush (stdin);
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
			printf("%s\n", message);
			printf("send %s to %s\n", message, address);
		}
		else {
			printf("not a correct input\n");
		}

	}

	return 0;
}

