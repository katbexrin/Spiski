#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>

int unavailable_proc_count = 0;
int ld = 0;
int res = 0;
int cd = 0;
int filesize = 0;
int fork_count_max = 3;
int pid = 0;
const int backlog = 10;
struct sockaddr_in saddr;
struct sockaddr_in caddr;
char *line = NULL;
size_t len = 0;
char *filepath = NULL;
size_t filepath_len = 0;
int empty_str_count = 0;
socklen_t size_saddr;
socklen_t size_caddr;
FILE *fd;
FILE *file;

void sig1() {
    	unavailable_proc_count++;
}

void sig2() {
   	unavailable_proc_count--;
}

void headers(int client, int size, int httpcode) {
	char buf[1024];
	char strsize[20];
	sprintf(strsize, "%d", size);
	if (httpcode == 200) {
		strcpy(buf, "HTTP/1.0 200 OK\r\n");
	}
	else if (httpcode == 404) {
		strcpy(buf, "HTTP/1.0 404 Not Found\r\n");
	}
	else if (httpcode == 503) {
		strcpy(buf, "HTTP/1.0 503 Service Unavailable\r\n");
	}
	else{
		strcpy(buf, "HTTP/1.0 500 Internal Server Error\r\n");
	}
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "Connection: keep-alive\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "Content-length: ");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, strsize);
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "simple-server");
	send(client, buf, strlen(buf), 0);
	sprintf(buf, "Content-Type: text/html\r\n");
	send(client, buf, strlen(buf), 0);
	strcpy(buf, "\r\n");
	send(client, buf, strlen(buf), 0);
}


void parseFileName(char *line, char **filepath, size_t *len) {
	char *start = NULL;
	while ((*line) != '/') line++;
	start = line + 1;
	while ((*line) != ' ') line++;
	(*len) = line - start;
	*filepath = (char*)malloc(*len + 1);
	*filepath = strncpy(*filepath, start, *len);
	(*filepath)[*len] = '\0';
	printf("%s \n", *filepath);
}

void client() {
	while(1) {
		printf("Process id which wait: %d\n", getpid());
		cd = accept(ld, (struct sockaddr *)&caddr, &size_caddr);
		kill(getppid(), SIGUSR1);
		if (cd == -1) {
			printf("accept error \n");
		}
		printf("Process id which works: %d\n", getpid());
		printf("client in %d descriptor. Client addr is %d \n", cd, caddr.sin_addr.s_addr);
		fd = fdopen(cd, "r");
		if (fd == NULL) {
			printf("error open client descriptor as file \n");
		}
		while ((res = getline(&line, &len, fd)) != -1) {
			if (strstr(line, "GET")) {
				parseFileName(line, &filepath, &filepath_len);
			}
			if (strcmp(line, "\r\n") == 0) {
				empty_str_count++;
			}
			else {
				empty_str_count = 0;
			}
			if (empty_str_count == 1) {
				break;
			}
			printf("%s", line);
		}
		printf("open %s \n", filepath);
		file = fopen(filepath, "r");
		if (file == NULL) {
			printf("404 File Not Found \n");
			headers(cd, 0, 404);
		}
		else {
			fseek(file, 0L, SEEK_END);
			filesize = ftell(file);
			fseek(file, 0L, SEEK_SET);
			headers(cd, filesize, 200);
			while (getline(&line, &len, file) != -1) {
				res = send(cd, line, len, 0);
				if (res == -1) {
					printf("send error \n");
				}
			}
		}
		while(1);
		close(cd);
		//while(1);
		kill(getppid(), SIGUSR2);
	}
}

int main() {
	signal(SIGUSR1,sig1); 
	signal(SIGUSR2,sig2);
	ld = socket(AF_INET, SOCK_STREAM, 0);
	if (ld == -1) {
		printf("listener create error \n");
	}
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(8080);
	saddr.sin_addr.s_addr = INADDR_ANY;
	res = bind(ld, (struct sockaddr *)&saddr, sizeof(saddr));
	if (res == -1) {
		printf("bind error \n");
	}
	res = listen(ld, backlog);
	if (res == -1) {
		printf("listen error \n");
	}
	int i;
	int temp = -1;
	for (i = 0; i < fork_count_max; i++) {
		if (i == 0 | pid != 0) {
			pid = fork();
		}
	}
	while (1) {
		if (pid == 0) {
			client();
		}
		else {
			if (unavailable_proc_count == fork_count_max) {
				printf("no free processes\r\n");
				cd = accept(ld, (struct sockaddr *)&caddr, &size_caddr);
				file = fopen("error.txt", "r");
				fseek(file, 0L, SEEK_END);
				filesize = ftell(file);
				fseek(file, 0L, SEEK_SET);
				headers(cd, filesize, 503);
				getline(&line, &len, file);
				res = send(cd, line, len, 0);
				close(cd);
			}
		}
		if (temp != unavailable_proc_count) {
			printf("unavailable_proc_count = %d\n", unavailable_proc_count);
			temp = unavailable_proc_count;
		}
	}
	return 0;
}

