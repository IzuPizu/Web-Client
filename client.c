
#include "helpers.h"
#include "parson.h"
#include "requests.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>		/* struct hostent, gethostbyname */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <stdbool.h>
#include <stdio.h>		/* printf, sprintf */
#include <stdlib.h>		/* exit, atoi, malloc, free */
#include <string.h>		/* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <unistd.h>		/* read, write, close */

#define HOST "34.246.184.49"
#define PORT 8080

/*prototypes*/
void register_account();
void login_account(char **cookies);
void enter_library(char *cookies, char **token);
void get_books(char *cookies, char *token);
void get_book(char *cookies, char *token);
void add_book(char *cookies, char *token);
void delete_book(char *cookies, char *token);
void logout_account(char **cookies, char **token);
void remove_newline(char *str);
void free_book_resources(char *title, char *author, char *genre, char *publisher, char *page_count);

/*enum for http status codes*/
enum HTTP_STATUS_CODE {
	HTTP_STATUS_OK = 200,
	HTTP_STATUS_CREATED = 201,
	HTTP_STATUS_NO_CONTENT = 204,
	HTTP_STATUS_BAD_REQUEST = 400,
	HTTP_STATUS_UNAUTHORIZED = 401,
	HTTP_STATUS_FORBIDDEN = 403,
	HTTP_STATUS_NOT_FOUND = 404,
	HTTP_STATUS_INTERNAL_SERVER_ERROR = 500,
};

int http_status_code;

int main(int argc, char *argv[])
{
	char *cookies = NULL;
	char *token = NULL;
	int sockfd;

	// boolean variables for exit and login status
	bool exit_status = false;
	bool login_status = false;

	// Open connection to server
	sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);

	// Read commands from stdin until exit command
	while (!exit_status) {
		char *command = malloc(LINELEN * sizeof(char));
		if (command == NULL) {
			exit(EXIT_FAILURE);
		}
		fgets(command, LINELEN, stdin);
		command = strtok(command, " \n");

		if (strcmp(command, "register") == 0) {
			if (login_status && cookies != NULL) {
				printf("ERROR:You are already logged in\n");
			} else {
				register_account();
			}
		} else if (strcmp(command, "login") == 0) {
			if (login_status && cookies != NULL) {
				printf("ERROR:You are already logged in\n");
			} else {
				login_account(&cookies);
				login_status = true;
			}
		} else if (strcmp(command, "enter_library") == 0) {
			if (cookies == NULL) {
				printf("ERROR:You are not logged in\n");
			} else {
				enter_library(cookies, &token);
			}
		} else if (strcmp(command, "get_books") == 0) {
			if (!login_status || cookies == NULL)
				printf("ERROR:You are not logged in or you don't have access to library\n");
			else
				get_books(cookies, token);

		} else if (strcmp(command, "get_book") == 0) {
			get_book(cookies, token);
		} else if (strcmp(command, "add_book") == 0) {
			add_book(cookies, token);
		} else if (strcmp(command, "delete_book") == 0) {
			delete_book(cookies, token);
		} else if (strcmp(command, "logout") == 0) {
			if (!login_status || cookies == NULL)
				printf("ERROR:You are not logged in\n");
			else {
				logout_account(&cookies, &token);
				login_status = false;
			}
		} else if (strcmp(command, "exit") == 0) {
			exit_status = true;
			login_status = false;
			close(sockfd);
		} else {
			printf("Invalid command.\n");
		}
		free(command);
	}

	close_connection(sockfd);
	free(cookies);
	free(token);

	return 0;
}

/*Remove newline from a string*/
void remove_newline(char *str)
{
	for (int i = 0; str[i] != '\0'; i++) {
		if (str[i] == '\n') {
			str[i] = '\0';
			break;
		}
	}
}

/*Register account function*/
void register_account()
{
	// Allocate memory for username and password
	char *username = malloc(LINELEN * sizeof(char));
	char *password = malloc(LINELEN * sizeof(char));
	if (username == NULL || password == NULL) {
		exit(EXIT_FAILURE);
	}

	// Read username and password from stdin
	printf("username=");
	fgets(username, LINELEN, stdin);
	remove_newline(username);

	printf("password=");
	fgets(password, LINELEN, stdin);
	remove_newline(password);

	// Validate username and password
	for (int i = 0; i < strlen(username); i++) {
		if (!isalnum(username[i]) || isspace(username[i])) {
			printf("ERROR:Username must only contain alphanumeric characters and no spaces.\n");
			free(username);
			free(password);
			return;
		}
	}

	for (int i = 0; i < strlen(password); i++) {
		if (isspace(password[i])) {
			printf("ERROR:Password cannot contain spaces.\n");
			free(username);
			free(password);
			return;
		}
	}

	// Create the JSON object
	JSON_Value *value = json_value_init_object();
	JSON_Object *object = json_value_get_object(value);
	char *string = NULL;
	json_object_set_string(object, "username", username);
	json_object_set_string(object, "password", password);
	string = json_serialize_to_string_pretty(value);

	// Create the POST request
	char *message =
		compute_post_request(HOST, "/api/v1/tema/auth/register/", "application/json", &string, 1, NULL, 1, NULL);
	int sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
	send_to_server(sockfd, message);
	// Receive the response from the server
	char *response = receive_from_server(sockfd);
	// Extract the status code from the response
	sscanf(response, "HTTP/1.1 %d", &http_status_code);

	// Check the status code and print corresponding message
	switch (http_status_code) {
	case HTTP_STATUS_CREATED:
		printf("SUCCESS:You have created a new account!\n");
		break;
	case HTTP_STATUS_UNAUTHORIZED:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_INTERNAL_SERVER_ERROR:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_BAD_REQUEST:
		printf("ERROR:%d Username already used!\n", http_status_code);
		break;
	case HTTP_STATUS_FORBIDDEN:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_NOT_FOUND:
		printf("ERROR:%d\n", http_status_code);
		break;
	default:
		printf("Status code: %d\n", http_status_code);
		break;
	}

	json_free_serialized_string(string);
	json_value_free(value);
	free(message);
	free(response);
	free(username);
	free(password);
}

/*Login account function*/
void login_account(char **cookies)
{
	// Allocate memory for username and password
	char *username = malloc(LINELEN * sizeof(char));
	char *password = malloc(LINELEN * sizeof(char));
	if (username == NULL || password == NULL) {
		exit(EXIT_FAILURE);
	}

	// Read username and password from stdin
	printf("username=");
	fgets(username, LINELEN, stdin);
	remove_newline(username);

	printf("password=");
	fgets(password, LINELEN, stdin);
	remove_newline(password);

	// Validate username and password
	for (int i = 0; i < strlen(username); i++) {
		if (!isalnum(username[i]) || isspace(username[i])) {
			printf("ERROR:Username must only contain alphanumeric characters and no spaces.\n");
			free(username);
			free(password);
			return;
		}
	}

	for (int i = 0; i < strlen(password); i++) {
		if (isspace(password[i])) {
			printf("ERROR:Password cannot contain spaces.\n");
			free(username);
			free(password);
			return;
		}
	}

	// Create the JSON object
	JSON_Value *value = json_value_init_object();
	JSON_Object *object = json_value_get_object(value);
	char *string = NULL;
	json_object_set_string(object, "username", username);
	json_object_set_string(object, "password", password);
	string = json_serialize_to_string_pretty(value);

	// Create the POST request using the JSON object
	char *message =
		compute_post_request(HOST, "/api/v1/tema/auth/login/", "application/json", &string, 1, NULL, 1, NULL);
	// Open connection to server
	int sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
	send_to_server(sockfd, message);
	// Receive the response from the server
	char *response = receive_from_server(sockfd);
	// Extract the status code from the response
	sscanf(response, "HTTP/1.1 %d", &http_status_code);
	char *set_cookie_header;

	// Check the status code and print corresponding message
	switch (http_status_code) {
	case HTTP_STATUS_OK:
		// Extract the cookies from the response
		set_cookie_header = strstr(response, "Set-Cookie:");
		if (set_cookie_header) {
			set_cookie_header += strlen("Set-Cookie: ");
			// Find the end of the cookie header
			char *cookie_end = strstr(set_cookie_header, "\r\n");
			if (cookie_end) {
				// Allocate memory for the cookies and find the length of the cookie header
				size_t cookie_length = cookie_end - set_cookie_header;
				*cookies = malloc(cookie_length + 1);
				// Check if memory was allocated successfully
				if (cookies == NULL) {
					exit(EXIT_FAILURE);
				}
				// Copy the cookie header
				if (*cookies) {
					strncpy(*cookies, set_cookie_header, cookie_length);
					remove_newline(*cookies);
				}
			}
		}
		printf("SUCCESS:You have logged in\n");
		break;
	case HTTP_STATUS_UNAUTHORIZED:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_INTERNAL_SERVER_ERROR:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_BAD_REQUEST:
		printf("ERROR:%d Username or password is incorrect\n", http_status_code);
		break;
	case HTTP_STATUS_FORBIDDEN:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_NOT_FOUND:
		printf("ERROR:%d\n", http_status_code);
		break;
	default:
		printf("Status code: %d\n", http_status_code);
		break;
	}

	json_free_serialized_string(string);
	json_value_free(value);
	free(message);
	free(response);
	free(username);
	free(password);
}

/*Enter library function*/
void enter_library(char *cookies, char **token)
{
	// Create the GET request using the cookies
	char *message = compute_get_request(HOST, "/api/v1/tema/library/access", NULL, &cookies, 1, NULL);
	// Open connection to server
	int sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
	send_to_server(sockfd, message);
	// Receive the response from the server
	char *response = receive_from_server(sockfd);
	// Extract the status code from the response
	sscanf(response, "HTTP/1.1 %d", &http_status_code);

	// Check the status code and print corresponding message
	switch (http_status_code) {
	case HTTP_STATUS_OK:
		printf("SUCCESS:You have now access to the library!\n");
		// Extract token to prove access to library
		char *token_start = strstr(response, "{\"token\":\"");
		if (token_start != NULL) {
			// Skip the  {"token":"  part
			token_start += strlen("{\"token\":\"");
			// Find the end of the token (we know it ends with a " character)
			char *token_end = strchr(token_start, '\"');
			if (token_end != NULL) {
				size_t token_length = token_end - token_start;
				// Allocate memory for the token
				*token = malloc(token_length + 1);
				// Check if memory was allocated successfully
				if (*token == NULL) {
					exit(EXIT_FAILURE);
				}
				// Copy the token
				strncpy(*token, token_start, token_length);
				remove_newline(*token);
			}
		}
		break;
	case HTTP_STATUS_UNAUTHORIZED:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_INTERNAL_SERVER_ERROR:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_BAD_REQUEST:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_FORBIDDEN:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_NOT_FOUND:
		printf("ERROR:%d\n", http_status_code);
		break;
	default:
		printf("Status code: %d\n", http_status_code);
		break;
	}

	free(message);
	free(response);
}

/*Get books function*/
void get_books(char *cookies, char *token)
{
	// Create the GET request using the token
	char *message = compute_get_request(HOST, "/api/v1/tema/library/books/", NULL, NULL, 1, token);
	// Open connection to server and send the request
	int sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
	send_to_server(sockfd, message);
	// Receive the response from the server
	char *response = receive_from_server(sockfd);
	// Extract the status code from the response
	sscanf(response, "HTTP/1.1 %d", &http_status_code);

	char *body_start = NULL;
	char *json_body = NULL;
	JSON_Value *json_value = NULL;
	JSON_Array *json_array = NULL;
	size_t count = 0;

	// Check the status code and print corresponding message
	switch (http_status_code) {
	case HTTP_STATUS_OK:
		// Find the start of the body
		body_start = strstr(response, "\r\n\r\n");
		if (body_start) {
			// Skip the "\r\n\r\n"
			json_body = body_start + 4;
		}
		// Parse the JSON body
		json_value = json_parse_string(json_body);
		json_array = json_value_get_array(json_value);
		// Get the number of books
		count = json_array_get_count(json_array);
		// Print the books
		printf("[\n");
		for (size_t i = 0; i < count; ++i) {
			JSON_Object *object = json_array_get_object(json_array, i);
			int id = (int)json_object_get_number(object, "id");
			const char *title = json_object_get_string(object, "title");
			printf("    {\n");
			printf("        \"id\": %d,\n", id);
			printf("        \"title\": \"%s\"\n", title);
			if (i < count - 1) {
				printf("    },\n");
			} else {
				printf("    }\n");
			}
		}
		printf("]\n");
		break;
	case HTTP_STATUS_UNAUTHORIZED:
		printf("ERROR:You are not logged in or you dont have access to the library\n");
		break;
	case HTTP_STATUS_INTERNAL_SERVER_ERROR:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_BAD_REQUEST:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_FORBIDDEN:
		printf("ERROR:You are not logged in or you dont have access to the library\n");
		break;
	case HTTP_STATUS_NOT_FOUND:
		printf("ERROR:%d\n", http_status_code);
		break;
	default:
		printf("Status code: %d\n", http_status_code);
		break;
	}

	free(message);
	free(response);
	close_connection(sockfd);
}

/*Get book function*/
void get_book(char *cookies, char *token)
{
	// Read the book id from stdin
	char *book_id = malloc(LINELEN * sizeof(char));
	if (book_id == NULL) {
		exit(EXIT_FAILURE);
	}
	printf("id=");
	fgets(book_id, LINELEN, stdin);
	remove_newline(book_id);

	if (cookies == NULL || token == NULL) {
		printf("ERROR:You are not logged in or you dont have access to the library\n");
		free(book_id);
		return;
	}

	char *address = malloc(LINELEN * sizeof(char));
	if (address == NULL) {
		exit(EXIT_FAILURE);
	}
	// Create the address for the GET request by concatenating the book id
	strcpy(address, "/api/v1/tema/library/books/");
	strcat(address, book_id);

	// Create the GET request using the token and full URL adress
	char *message = compute_get_request(HOST, address, NULL, NULL, 1, token);
	int sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
	// Send the request to the server
	send_to_server(sockfd, message);
	// Receive the response from the server
	char *response = receive_from_server(sockfd);
	sscanf(response, "HTTP/1.1 %d", &http_status_code);

	char *body_start = NULL;
	char *json_body = NULL;
	JSON_Value *json_value = NULL;

	// Check the status code and print corresponding message
	switch (http_status_code) {
	case HTTP_STATUS_OK:
		body_start = strstr(response, "\r\n\r\n");
		if (body_start) {
			// Skip the "\r\n\r\n"
			json_body = body_start + 4;
		}
		// Parse the JSON body
		json_value = json_parse_string(json_body);
		// Check if the JSON body is an object
		if (json_value_get_type(json_value) == JSONObject) {
			JSON_Object *object = json_value_get_object(json_value);
			// Extract the book details and print them
			int id = (int)json_object_get_number(object, "id");
			const char *title = json_object_get_string(object, "title");
			const char *author = json_object_get_string(object, "author");
			const char *publisher = json_object_get_string(object, "publisher");
			const char *genre = json_object_get_string(object, "genre");
			int page_count = (int)json_object_get_number(object, "page_count");

			printf("{\n");
			printf("    \"id\": %d,\n", id);
			printf("    \"title\": \"%s\",\n", title);
			printf("    \"author\": \"%s\",\n", author);
			printf("    \"publisher\": \"%s\",\n", publisher);
			printf("    \"genre\": \"%s\",\n", genre);
			printf("    \"page_count\": %d\n", page_count);
			printf("}\n");
		}
		break;
	case HTTP_STATUS_UNAUTHORIZED:
		printf("ERROR:You are not logged in or you dont have access to the library\n");
		break;
	case HTTP_STATUS_INTERNAL_SERVER_ERROR:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_BAD_REQUEST:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_FORBIDDEN:
		printf("ERROR:You are not logged in or you dont have access to the library\n");
		break;
	case HTTP_STATUS_NOT_FOUND:
		printf("ERROR:%d Book with ID:%d was not found \n", http_status_code, atoi(book_id));
		break;
	default:
		printf("Status code: %d\n", http_status_code);
		break;
	}

	free(message);
	free(response);
	free(address);
	free(book_id);
}

/*Free resources for book*/
void free_book_resources(char *title, char *author, char *genre, char *publisher, char *page_count)
{
	free(title);
	free(author);
	free(genre);
	free(publisher);
	free(page_count);
}

/* Add book function */
void add_book(char *cookies, char *token)
{
	char *title = NULL;
	char *author = NULL;
	char *genre = NULL;
	char *page_count = NULL;
	char *publisher = NULL;
	char *message = NULL;

	// Allocate memory for the book fields and read them from stdin
	title = (char *)malloc(LINELEN * sizeof(char));
	if (title == NULL) {
		exit(EXIT_FAILURE);
	}

	printf("title=");
	fgets(title, LINELEN, stdin);
	remove_newline(title);

	author = (char *)malloc(LINELEN * sizeof(char));
	if (author == NULL) {
		exit(EXIT_FAILURE);
	}

	printf("author=");
	fgets(author, LINELEN, stdin);
	remove_newline(author);

	genre = (char *)malloc(LINELEN * sizeof(char));
	if (genre == NULL) {
		exit(EXIT_FAILURE);
	}

	printf("genre=");
	fgets(genre, LINELEN, stdin);
	remove_newline(genre);

	publisher = (char *)malloc(LINELEN * sizeof(char));
	if (publisher == NULL) {
		exit(EXIT_FAILURE);
	}

	printf("publisher=");
	fgets(publisher, LINELEN, stdin);
	remove_newline(publisher);

	page_count = (char *)malloc(LINELEN * sizeof(char));
	if (page_count == NULL) {
		exit(EXIT_FAILURE);
	}

	printf("page_count=");
	fgets(page_count, LINELEN, stdin);
	remove_newline(page_count);

	if (cookies == NULL || token == NULL) {
		printf("ERROR:You are not logged in or you don't have access to library\n");
		free_book_resources(title, author, genre, publisher, page_count);
		return;
	}

	// Array of fields to check if they are empty
	char *fields[] = {title, author, genre, publisher};

	// Check if all the fields are completed
	for (int i = 0; i < 4; ++i) {
		if (fields[i] == NULL || strlen(fields[i]) == 0) {
			printf("ERROR: Please complete all the fields!\n");
			free_book_resources(title, author, genre, publisher, page_count);
			return;
		}
	}

	// Check if page_count is a valid number
	if (atoi(page_count) <= 0) {
		printf("ERROR:Invalid page count\n");
		free_book_resources(title, author, genre, publisher, page_count);
		return;
	}

	// Create the JSON object
	JSON_Value *value = json_value_init_object();
	JSON_Object *object = json_value_get_object(value);
	json_object_set_string(object, "title", title);
	json_object_set_string(object, "author", author);
	json_object_set_string(object, "genre", genre);
	json_object_set_string(object, "publisher", publisher);
	json_object_set_number(object, "page_count", atoi(page_count));
	char *string = json_serialize_to_string_pretty(value);

	// Open connection to server
	int sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);

	// Create the Authorization header using the JWT token
	char *auth_header = malloc(LINELEN * sizeof(char));
	if (auth_header == NULL) {
		exit(EXIT_FAILURE);
	}
	sprintf(auth_header, "Authorization: Bearer %s", token);
	char *headers[] = {auth_header};

	// Create the POST request using the JSON object and the token
	message =
		compute_post_request(HOST, "/api/v1/tema/library/books", "application/json", &string, 1, headers, 1, token);
	// Send the request to the server
	send_to_server(sockfd, message);
	// Receive the response from the server
	char *response = receive_from_server(sockfd);
	// Extract the status code from the response
	sscanf(response, "HTTP/1.1 %d", &http_status_code);

	// Check the status code and print corresponding message
	switch (http_status_code) {
	case HTTP_STATUS_OK:
		printf("SUCCESS:You have added the book!\n");
		break;
	case HTTP_STATUS_UNAUTHORIZED:
		printf("ERROR:You are not logged in or you dont have access to the library\n");
		break;
	case HTTP_STATUS_INTERNAL_SERVER_ERROR:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_BAD_REQUEST:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_FORBIDDEN:
		printf("ERROR:You are not logged in or you dont have access to the library\n");
		break;
	case HTTP_STATUS_NOT_FOUND:
		printf("ERROR:%d\n", http_status_code);
		break;
	default:
		printf("Status code: %d\n", http_status_code);
		break;
	}

	free(message);
	free(response);
	free(title);
	free(author);
	free(genre);
	free(publisher);
	json_free_serialized_string(string);
	json_value_free(value);
	free(auth_header);
}

/* Delete book function */
void delete_book(char *cookies, char *token)
{
	// Read the book id from stdin
	char *id = malloc(LINELEN * sizeof(char));
	if (id == NULL) {
		exit(EXIT_FAILURE);
	}
	printf("id=");
	fgets(id, LINELEN, stdin);
	remove_newline(id);

	if (cookies == NULL || token == NULL) {
		printf("ERROR:You are not logged in or you don't have access to library\n");
		free(id);
		return;
	}

	// Create the address for the DELETE request by concatenating the book id
	char *address = malloc(LINELEN * sizeof(char));
	if (address == NULL) {
		exit(EXIT_FAILURE);
	}
	strcpy(address, "/api/v1/tema/library/books/");
	strcat(address, id);

	// Create the DELETE request using the token and full URL address
	char *message = compute_delete_request(HOST, address, NULL, NULL, 1, token);
	int sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
	send_to_server(sockfd, message);
	// Receive the response from the server
	char *response = receive_from_server(sockfd);
	// Extract the status code from the response
	sscanf(response, "HTTP/1.1 %d", &http_status_code);

	// Check the status code and print corresponding message
	switch (http_status_code) {
	case HTTP_STATUS_OK:
		printf("SUCCESS:You have deleted the book!\n");
		break;
	case HTTP_STATUS_UNAUTHORIZED:
		printf("ERROR:You are not logged in or you dont have access to the library\n");
		break;
	case HTTP_STATUS_INTERNAL_SERVER_ERROR:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_BAD_REQUEST:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_FORBIDDEN:
		printf("ERROR:You are not logged in or you dont have access to the library\n");
		break;
	case HTTP_STATUS_NOT_FOUND:
		printf("ERROR:%d\n", http_status_code);
		break;
	default:
		printf("Status code: %d\n", http_status_code);
		break;
	}

	free(message);
	free(response);
	free(address);
	free(id);
}

/* Logout account function */
void logout_account(char **cookies, char **token)
{
	// Create the GET request using the token
	char *message = compute_get_request(HOST, "/api/v1/tema/auth/logout/", NULL, cookies, 1, *token);
	// Open connection to server and send the request
	int sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
	send_to_server(sockfd, message);

	// Receive the response from the server
	char *response = receive_from_server(sockfd);
	// Extract the status code from the response
	sscanf(response, "HTTP/1.1 %d", &http_status_code);

	// Check the status code and print corresponding message
	switch (http_status_code) {
	case HTTP_STATUS_OK:
		printf("SUCCESS:You have logged out\n");
		break;
	case HTTP_STATUS_UNAUTHORIZED:
		printf("ERROR:You are not logged in\n");
		break;
	case HTTP_STATUS_INTERNAL_SERVER_ERROR:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_BAD_REQUEST:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_FORBIDDEN:
		printf("ERROR:%d\n", http_status_code);
		break;
	case HTTP_STATUS_NOT_FOUND:
		printf("ERROR:%d\n", http_status_code);
		break;
	default:
		printf("Status code: %d\n", http_status_code);
		break;
	}

	free(*cookies);
	*cookies = NULL;
	free(*token);
	*token = NULL;
	free(message);
	free(response);
}