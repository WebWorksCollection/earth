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

struct http_header {
	char *key;
	char *value;
};

struct http_request {
	char *method;
	char *path;
	char *protocol;
	size_t headers_length;
	struct http_header *headers;
	char *body;
};

void http_request_free(struct http_request *request) {
	free(request->method);
	free(request->path);
	free(request->protocol);

	for(size_t i = 0; i < request->headers_length; i++) {
		free(request->headers[i].key);
		free(request->headers[i].value);
	}

	free(request->headers);
	free(request->body);
}

int http_request_parse(struct http_request *request, char *buffer) {
	// method

	char *method_start = buffer;
	char *method_end = strchr(method_start, ' ');
	size_t method_length = method_end - method_start;

	request->method = malloc(method_length + 1);
	strncpy(request->method, method_start, method_length);
	request->method[method_length] = 0;

	// path

	char *path_start = method_end + 1;
	char *path_end = strchr(path_start, ' ');
	size_t path_length = path_end - path_start;

	request->path = malloc(path_length + 1);
	strncpy(request->path, path_start, path_length);
	request->path[path_length] = 0;

	// protocol

	char *protocol_start = path_end + 1;
	char *protocol_end = strchr(protocol_start, '\n');
	size_t protocol_length = protocol_end - protocol_start;

	request->protocol = malloc(protocol_length + 1);
	strncpy(request->protocol, protocol_start, protocol_length);
	request->protocol[protocol_length] = 0;

	// headers

	char *headers_start = protocol_end + 1;
	ssize_t headers_length = 0;

	for(char *c = headers_start; *c != 0; c++) {
		if(*c == '\n') {
			headers_length++;
		}
	}

	request->headers_length = --headers_length;

	request->headers = calloc(headers_length, sizeof(struct http_header));

	char *key;
	char *key_start;
	char *key_end;
	size_t key_length;

	char *value;
	char *value_start;
	char *value_end = protocol_end;
	size_t value_length;

	size_t content_length = 0;

	for(size_t i = 0; i < headers_length; i++) {
		key_start = value_end + 1;
		key_end = strchr(key_start, ':');
		key_length = key_end - key_start;

		key = malloc(key_length + 1);
		strncpy(key, key_start, key_length);
		key[key_length] = 0;

		for(size_t j = 0; j < key_length; j++) {
			key[j] = tolower(key[j]);
		}

		request->headers[i].key = key;

		value_start = key_end + 1;

		if(*value_start == ' ')
			value_start++;

		value_end = strchr(value_start, '\n');
		value_length = value_end - value_start;

		value = malloc(value_length + 1);
		strncpy(value, value_start, value_length);
		value[value_length] = 0;

		request->headers[i].value = value;

		if(content_length == 0 && strcmp(key, "content-length") == 0) {
			sscanf(value, "%lu", &content_length);
		}
	}

	if(content_length) {
		char *body_start = strchr(value_end + 1, '\n') + 1;

		if(strlen(body_start) < content_length) {
			printf("length less than content length\n");
		}

		request->body = malloc(content_length + 1);
		strncpy(request->body, body_start, content_length);
		request->body[content_length] = 0;
	}

	return 0;
}


void handle_connection(void *data) {
	int fd = (int) data;

	size_t buffer_size = 1024;
	size_t buffer_length = 0;
	char *buffer = calloc(buffer_size + 1, 1);

	if(buffer == NULL) {
		error("malloc");
	}

	while(1) {
		ssize_t r = recv(fd, buffer + buffer_length, buffer_size - buffer_length, 0);

		if(r > 0) {
			buffer_length += r;
		} else {
			error("recv");
		}

		int end = 0;

		for(size_t i = 3; i < buffer_length; i++) {
			if(buffer[i] == '\n' && (buffer[i - 1] == '\n' || buffer[i - 2] == '\n')) {
				end = 1;
			}
		}

		if(end == 1) {
			break;
		}
	}

	struct http_request req;

	http_request_parse(&req, buffer);

	printf("request->method '%s'\n", req.method);
	printf("request->path '%s'\n", req.path);
	printf("request->protcol '%s'\n", req.protocol);

	printf("request->headers_length '%lu'\n", req.headers_length);

	for(size_t i = 0; i < req.headers_length; i++) {
		printf("request->header[%lu] '%s' '%s'\n", i, req.headers[i].key, req.headers[i].value);
	}

	printf("request->body '%s'\n", req.body);

	char *index = "index.html";

	if(req.path[strlen(req.path) - 1] == '/') {
		req.path = realloc(req.path, strlen(req.path) + strlen(index) + 1);
		strcat(req.path, index);
	}

	char *public = "public";
	char *filepath = calloc(strlen(public) + strlen(req.path) + 1, 1);

	strcat(filepath, public);
	strcat(filepath, req.path);

	printf("filepath '%s'\n", filepath);

	FILE *file = fopen(filepath, "r");

	fseek(file, 0L, SEEK_END);
	size_t file_length = ftell(file);

	char *headers = calloc(1024, 1);

	if(headers == NULL) {
		error("malloc");
	}

	size_t headers_length = sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Length: %lu\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n", file_length);

	send(fd, headers, headers_length, 0);

	size_t output_buffer_size = 1024 * 1024;
	size_t output_buffer_length = 0;
	char *output_buffer = malloc(output_buffer_size);

	if(output_buffer == NULL) {
		error("malloc");
	}

	rewind(file);

	while(1) {
		output_buffer_length = fread(output_buffer, 1, output_buffer_size, file);

		if(output_buffer_length == 0) {
			return;
		}

		send(fd, output_buffer, output_buffer_length, 0);

		if(output_buffer_length != output_buffer_size) {
			return;
		}
	}
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
