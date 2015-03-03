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

	return 0;

	// return getInput();
}

int getInput() {
	char msg[512];

	while (1) {
		fflush (stdin);
		printf("Enter message: \n");
		gets(msg);



		printf("You type: %s\n",msg);
	}

	return 0;
}