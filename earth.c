#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <endian.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

void error(char *reason) {
	perror(reason);
	exit(1);
}

void handle_connection(void *data) {
	int fd = (int) data;
}

int main() {
	int fd = socket(AF_INET, SOCK_STREAM, 0);

	if(fd < 0) {
		error("socket");
	}

	int on = 1;

	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		error("setsockopt");
	}

	struct sockaddr_in servaddr;

	memset(&servaddr, 0, sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(8000);

	if(bind(fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		error("bind");
	}

	if(listen(fd, SOMAXCONN) < 0) {
		error("listen");
	}

	while(1) {
		int connfd = accept(fd, (struct sockaddr *) NULL, NULL);

		pthread_t thread;

		pthread_create(&thread, NULL, handle_connection, (void *) connfd);
	}
}
