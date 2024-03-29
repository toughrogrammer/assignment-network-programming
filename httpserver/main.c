#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "data_structure.h"
#include "util.h"

#define SERVER_STRING "Server: httpserver/0.0.1\r\n"
#define ARGV_INDEX_PORT 1
#define FILE_INFO_JSON_FORMAT "{ \"%s\" : \"%s\" \"%s\" : \"%s\" \"%s\" : \"%s\" \"%s\" : \"%s\" \"%s\" : \"%s\" \"%s\" : \"%s\" \"%s\" : \"%s\" },"


int get_list_of_files(const char* path, char* content);
int get_content_of_file(const char* path, char* content);
int handle_request(int sock);
int init_listening_socket(struct sockaddr_in* addr, int port);
void response_200(int sock, 
	const char* extra_header, 
	const char* content);
void response_200_json(int sock, 
	const char* extra_header, 
	const char* content);
void response_201(int sock, 
	const char* extra_header, 
	const char* content);
void response_400(int sock);
void response_404(int sock);
void response_405(int sock);


void random_string(char *dest, int length) {
	static const char alphanum[] =
		"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	
	for (int i = 0; i < length; ++i) {
		dest[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
	}
	
	dest[length] = 0;
}

int is_directory(const struct stat stat_buffer) {
	return (stat_buffer.st_mode & S_IFMT) == S_IFDIR;
}

int is_file(const struct stat stat_buffer) {
	return (stat_buffer.st_mode & S_IFMT) == S_IFREG;
}

int get_list_of_files(const char* path, char* content) {
	char process_str[MAX_LENGTH];
	sprintf(process_str, "ls -al %s", path);
	printf("%s\n", process_str);
	
	char buffer[MAX_LENGTH];
	FILE *process_fp = popen(process_str, "r");
	if (process_fp == NULL)	{
		return -1;
	}
	
	char json_array_inside[MAX_LENGTH] = { 0, };
	{
		fgets(buffer, MAX_LENGTH, process_fp);
		while(fgets(buffer, MAX_LENGTH, process_fp) != NULL) {
			char *str_permission = strtok(buffer, " ");
			char *str_link = strtok(NULL, " ");
			char *str_owner = strtok(NULL, " ");
			char *str_group = strtok(NULL, " ");
			char *str_size = strtok(NULL, " ");
			char *str_time = strtok(NULL, " ");
			strtok(NULL, " ");
			strtok(NULL, " ");
			char *str_name = strtok(NULL, " \n");
			
			char tmp[MAX_LENGTH];
			sprintf(tmp, FILE_INFO_JSON_FORMAT,
					"permission", str_permission,
					"link", str_link,
					"owner", str_owner,
					"group", str_group,
					"size", str_size,
					"time", str_time,
					"name", str_name);
			strcat(json_array_inside, tmp);
		}
		int length_json_array_inside = (int)strlen(json_array_inside);
		if( length_json_array_inside > 0 ) {
			json_array_inside[length_json_array_inside - 1] = 0;
		}
	}
	pclose(process_fp);
	sprintf(content, "[%s]", json_array_inside);

	return 1;
}

int get_list_of_files_searched(const char* path, const char* query, char* content) {
	char process_str[MAX_LENGTH];
	sprintf(process_str, "ls -al %s", path);
	printf("%s\n", process_str);
	printf("query : %s\n", query);
	
	char buffer[MAX_LENGTH];
	FILE *process_fp = popen(process_str, "r");
	if (process_fp == NULL)	{
		return -1;
	}

	char json_array_inside[MAX_LENGTH] = { 0, };
	memset(json_array_inside, 0, sizeof(json_array_inside));
	{
		fgets(buffer, MAX_LENGTH, process_fp);
		while(fgets(buffer, MAX_LENGTH, process_fp) != NULL) {
			char *str_permission = strtok(buffer, " ");
			char *str_link = strtok(NULL, " ");
			char *str_owner = strtok(NULL, " ");
			char *str_group = strtok(NULL, " ");
			char *str_size = strtok(NULL, " ");
			char *str_time = strtok(NULL, " ");
			strtok(NULL, " ");
			strtok(NULL, " ");
			char *str_name = strtok(NULL, " \n");

			char curr_file_path[MAX_LENGTH];
			sprintf(curr_file_path, "%s/%s", path, str_name);
			FILE *curr_file = fopen(curr_file_path, "r");
			if( ! curr_file ) {
				fclose(curr_file);
				return -1;
			}

			char file_content[MAX_LENGTH] = { 0, };
			char line[MAX_LENGTH];
			while(fgets(line, MAX_LENGTH, curr_file) != NULL) {
				strcat(file_content, line);
			}

			fclose(curr_file);

			if( strstr(file_content, query) == NULL ) {
				continue;
			}
			
			char tmp[MAX_LENGTH];
			sprintf(tmp, FILE_INFO_JSON_FORMAT,
					"permission", str_permission,
					"link", str_link,
					"owner", str_owner,
					"group", str_group,
					"size", str_size,
					"time", str_time,
					"name", str_name);
			strcat(json_array_inside, tmp);
		}
		int length_json_array_inside = (int)strlen(json_array_inside);
		if( length_json_array_inside > 0 ) {
			json_array_inside[length_json_array_inside - 1] = 0;
		}
	}
	pclose(process_fp);
	sprintf(content, "[%s]", json_array_inside);
	
	return 1;
}

int get_content_of_file(const char* path, char* content) {
	printf("%s\n", path);
	FILE *fp = fopen(path, "r");
	if( !fp ) {
		fclose(fp);
		return -1;
	}

	char buffer[MAX_LENGTH];
	while(fgets(buffer, MAX_LENGTH, fp) != NULL) {
		strcat(content, buffer);
	}
	
	fclose(fp);
	
	return 1;
}

int process_request_get(const struct http_request* request) {
	char path[32];
	sprintf(path, ".%s", request->url);
	
	struct stat stat_buffer;
	if( stat(path, &stat_buffer) == -1 ) {
		response_404(request->sock);
		return -1;
	}
	
	char content[MAX_LENGTH] = { 0, };
	if( (stat_buffer.st_mode & S_IFMT) == S_IFDIR ) {
		if( get_list_of_files(path, content) < 0 ) {
			response_404(request->sock);
			return -1;
		}

		response_200_json(request->sock, "", content);
		return 1;
	} else if( (stat_buffer.st_mode & S_IFMT) == S_IFREG ) {
		if( get_content_of_file(path, content) < 0 ) {
			printf("file not exist\n");
			response_404(request->sock);
			return -1;
		}

		response_200(request->sock, "", content);
		return 1;
	}
	
	
	response_400(request->sock);
	return -1;
}

int process_request_post(const struct http_request* request) {
	char path[64];
	sprintf(path, ".%s", request->url);

	for(int i = 0; i < request->header_count; i ++ ) {
		printf("%s\n", request->headers[i]);
	}

	char content_type[MAX_LENGTH] = { 0, };
	find_header_value(request, "Content-Type", content_type);
	if( strcmp(content_type, "application/x-www-form-urlencoded") == 0 ) {
		if( request->body[0] == 'q' && request->body[1] == '=' ) {
			// searching
			char content[MAX_LENGTH] = { 0, };
			get_list_of_files_searched(path, request->body + 2, content);
			response_200(request->sock, "", content);
			return 1;
		}

		response_405(request->sock);
		return -1;
	}
	
	struct stat stat_buffer;
	if( stat(path, &stat_buffer) == -1 ) {
		FILE *new_fp = fopen(path, "w");
		if( ! new_fp ) {
			fclose(new_fp);
			// try to create file at not be existed directory
			// or some other reasons...
			response_405(request->sock);
			return -1;
		}

		fprintf(new_fp, "%s", request->body);
		fclose(new_fp);
		
		char host_url[MAX_LENGTH];
		find_header_value(request, "Host", host_url);
		
		char location_part[MAX_LENGTH];
		sprintf(location_part, "Location: %s%s\r\n", host_url, path + 1);

		response_201(request->sock, location_part, request->body);
		return 1;
	}
	
	if( is_directory(stat_buffer) ) {
		char new_filename[16];
		random_string(new_filename, 8);
		
		sprintf(path, ".%s/%s", request->url, new_filename);
		
		FILE *new_fp = fopen(path, "w");
		fprintf(new_fp, "%s", request->body);
		fclose(new_fp);
		
		char host_url[MAX_LENGTH] = { 0, };
		find_header_value(request, "Host", host_url);
		
		char location_part[MAX_LENGTH];
		sprintf(location_part, "Location: %s%s\r\n", host_url, path + 1);

		response_201(request->sock, location_part, request->body);
		return 1;
	} else if( is_file(stat_buffer) ) {
		FILE *fp = fopen(path, "a");
		fprintf(fp, "%s", request->body);
		fclose(fp);
		
		response_200(request->sock, "", request->body);
		return 1;
	}
	
	response_400(request->sock);
	return -1;
}

int process_request_put(const struct http_request* request) {
	char path[64];
	sprintf(path, ".%s", request->url);
	
	struct stat stat_buffer;
	if( stat(path, &stat_buffer) == -1 ) {
		if( strlen(request->body) == 0 ) {
			// create new directory
			char process_str[MAX_LENGTH];
			sprintf(process_str, "mkdir -p %s", path);
			printf("%s\n", process_str);
			
			char buffer[MAX_LENGTH];
			FILE *process_fp = popen(process_str, "r");
			if (process_fp == NULL)	{
				return -1;
			}
			pclose(process_fp);
			
			response_201(request->sock, "", "");
			return 1;
		} else {
			FILE *fp = fopen(path, "w");
			if( ! fp ) {
				fclose(fp);
				response_400(request->sock);
			}

			fprintf(fp, "%s", request->body);

			fclose(fp);

			response_201(request->sock, "", request->body);
			return 1;
		}
	}
	
	if( is_file(stat_buffer) ) {
		FILE *fp = fopen(path, "w");
		fprintf(fp, "%s", request->body);
		fclose(fp);

		response_200(request->sock, "", request->body);
		return 1;
	}

	if( is_directory(stat_buffer) && strlen(request->body) > 0 ) {
		response_405(request->sock);
		return -1;
	}
	
	response_400(request->sock);
	return -1;
}

int process_request_delete(const struct http_request* request) {
	char path[64];
	sprintf(path, ".%s", request->url);
	
	struct stat stat_buffer;
	if( stat(path, &stat_buffer) == -1 ) {
		response_404(request->sock);
		return -1;
	}
	
	if( ! is_file(stat_buffer) ) {
		response_400(request->sock);
		return -1;
	}
	
	char process_str[MAX_LENGTH];
	sprintf(process_str, "rm -f %s", path);
	printf("%s\n", process_str);
	
	char buffer[MAX_LENGTH];
	FILE *process_fp = popen(process_str, "r");
	if (process_fp == NULL)	{
		return -1;
	}
	pclose(process_fp);
	
	response_200(request->sock, "", "");
	return 1;
}

int process_request(const struct http_request* request) {
	if( strcmp(request->method, "GET") == 0 ) {
		return process_request_get(request);
	} else if( strcmp(request->method, "POST") == 0 ) {
		return process_request_post(request);
	} else if( strcmp(request->method, "PUT") == 0 ) {
		return process_request_put(request);
	} else if( strcmp(request->method, "DELETE") == 0 ) {
		return process_request_delete(request);
	}
	
	response_400(request->sock);
	return -1;
}

void parsing_http_request(struct http_request* request, char* message) {
	char *last = message;
	char header_part[MAX_LENGTH_HEADER] = { 0, };
	
	last = tokenizing_multi_character_delim(request->method, last, " ");
	last = tokenizing_multi_character_delim(request->url, last, " ");
	last = tokenizing_multi_character_delim(request->version, last, "\r\n");
	last = tokenizing_multi_character_delim(header_part, last, "\r\n\r\n");
	last = tokenizing_multi_character_delim(request->body, last, "\r\n");
	
	request->header_count = 0;
	last = header_part;
	while(1) {
		last = tokenizing_multi_character_delim(request->headers[request->header_count], last, "\r\n");
		request->header_count++;
		if( last == NULL ) {
			break;
		}
	}
}

int read_all_data(int sock_client, char* buffer) {
	ssize_t len = recv(sock_client, buffer, MAX_LENGTH, 0);
	return (int)len;
}

int handle_request(int sock) {
	char str[MAX_LENGTH] = { 0, };
	if( ! read_all_data(sock, str) ) {
		return -1;
	}
	
	http_request request;
	memset(&request, 0, sizeof(request));

	request.sock = sock;
	parsing_http_request(&request, str);
	
	char response[MAX_LENGTH] = { 0, };
	if( process_request(&request) < 0 ) {
		fprintf(stderr, "failed to process request\n");
	}

	shutdown(sock, SHUT_WR);
	clear_recv_buffer(sock);
	close(sock);

	return 1;
}

int init_listening_socket(struct sockaddr_in* addr, int port) {
	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = htons(INADDR_ANY);
	addr->sin_port = htons(port);

	int sock_listen = socket(AF_INET, SOCK_STREAM, 0);
	if( sock_listen < 0 ) {
		ERROR_LOGGING("failed to create listening socket")
	}

	int opt_sock_reuse = 1;
	if( setsockopt(sock_listen, SOL_SOCKET, SO_REUSEADDR, (char *)&opt_sock_reuse, (int)sizeof(opt_sock_reuse)) ) {
		ERROR_LOGGING("failed to setsockopt")
	}

	return sock_listen;
}

int main(int argc, char const *argv[])
{
	if( argc <= 1 ) {
		ERROR_LOGGING("usage:\narg1 : port");
	}

	printf("start\n");
	
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	int sock_listen = init_listening_socket(&servaddr, atoi(argv[ARGV_INDEX_PORT]));

	bind(sock_listen, (struct sockaddr *) &servaddr, sizeof(servaddr));
	
	listen(sock_listen, 10);
	printf("start listen\n");
	
	while(1) {
		int sock_client = accept(sock_listen, (struct sockaddr*) NULL, NULL);
		if( sock_client < 0 ) {
			fprintf(stderr, "failed to accept client socket\n");
			continue;
		}

		if( handle_request(sock_client) < 0 ) {
			fprintf(stderr, "failed to handle request\n");
			continue;
		}
	}
	
	return 0;
}

void response_200(int sock, const char* extra_header, const char* content) {
	char response[MAX_LENGTH];
	sprintf(response, "HTTP/1.1 200 OK\r\n");
	send(sock, response, strlen(response), 0);
	sprintf(response, SERVER_STRING);
	send(sock, response, strlen(response), 0);
	sprintf(response, "Content-Type: text/plain; charset=utf-8\r\n");
	send(sock, response, strlen(response), 0);
	if( extra_header != NULL && strlen(extra_header) > 0 ) {
		sprintf(response, "%s\r\n", extra_header);
		send(sock, response, strlen(response), 0);
	}
	sprintf(response, "\r\n");
	send(sock, response, strlen(response), 0);
	if( content != NULL && strlen(content) > 0 ) {
		sprintf(response, "%s\r\n", content);
		send(sock, response, strlen(response), 0);
	}
}

void response_200_json(int sock, const char* extra_header, const char* content) {
	char response[MAX_LENGTH];
	sprintf(response, "HTTP/1.1 200 OK\r\n");
	send(sock, response, strlen(response), 0);
	sprintf(response, SERVER_STRING);
	send(sock, response, strlen(response), 0);
	sprintf(response, "Content-Type: %s\r\n", "application/json");
	send(sock, response, strlen(response), 0);
	if( extra_header != NULL && strlen(extra_header) > 0 ) {
		sprintf(response, "%s\r\n", extra_header);
		send(sock, response, strlen(response), 0);
	}
	sprintf(response, "\r\n");
	send(sock, response, strlen(response), 0);
	if( content != NULL && strlen(content) > 0 ) {
		sprintf(response, "%s\r\n", content);
		send(sock, response, strlen(response), 0);
	}
}

void response_201(int sock, const char* extra_header, const char* content) {
	char response[MAX_LENGTH];
	sprintf(response, "HTTP/1.1 201 Created\r\n");
	send(sock, response, strlen(response), 0);
	sprintf(response, SERVER_STRING);
	send(sock, response, strlen(response), 0);
	sprintf(response, "Content-Type: text/plain; charset=utf-8\r\n");
	send(sock, response, strlen(response), 0);
	if( extra_header != NULL && strlen(extra_header) > 0 ) {
		sprintf(response, "%s\r\n", extra_header);
		send(sock, response, strlen(response), 0);
	}
	sprintf(response, "\r\n");
	send(sock, response, strlen(response), 0);
	if( content != NULL && strlen(content) > 0 ) {
		sprintf(response, "%s\r\n", content);
		send(sock, response, strlen(response), 0);
	}
}

void response_400(int sock) {
	char response[MAX_LENGTH];
	sprintf(response, "HTTP/1.1 400 Bad Request\r\n");
	send(sock, response, strlen(response), 0);
	sprintf(response, SERVER_STRING);
	send(sock, response, strlen(response), 0);
	sprintf(response, "Content-Type: text/plain; charset=utf-8\r\n");
	send(sock, response, strlen(response), 0);
	sprintf(response, "\r\n");
	send(sock, response, strlen(response), 0);
}

void response_404(int sock) {
	char response[MAX_LENGTH];
	sprintf(response, "HTTP/1.1 404 Not Found\r\n");
	send(sock, response, strlen(response), 0);
	sprintf(response, SERVER_STRING);
	send(sock, response, strlen(response), 0);
	sprintf(response, "Content-Type: text/plain; charset=utf-8\r\n");
	send(sock, response, strlen(response), 0);
	sprintf(response, "\r\n");
	send(sock, response, strlen(response), 0);
}

void response_405(int sock) {
	char response[MAX_LENGTH];
	sprintf(response, "HTTP/1.1 405 Method not allowed\r\n");
	send(sock, response, strlen(response), 0);
	sprintf(response, SERVER_STRING);
	send(sock, response, strlen(response), 0);
	sprintf(response, "Content-Type: text/plain; charset=utf-8\r\n");
	send(sock, response, strlen(response), 0);
	sprintf(response, "\r\n");
	send(sock, response, strlen(response), 0);
}