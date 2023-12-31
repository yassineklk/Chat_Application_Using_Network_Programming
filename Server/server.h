#ifndef SERVER_H
#define SERVER_H

#ifdef WIN32

#include <winsock2.h>

#elif defined(linux)

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <unistd.h> /* close */
#include <netdb.h>  /* gethostbyname */
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;

#else

#error not defined for this platform

#endif

#define CRLF "\r\n"
#define PORT 1977
#define MAX_CLIENTS 100

#define BUF_SIZE 1024

#include "client.h"

static enum COMMAND { NO_COMMAND,
                      PRIVATE_MSG_COMMAND,
                      CHANGE_GROUP_COMMAND,
                      UNKNOWN_COMMAND };

static void init(void);
static void end(void);
static void app(void);
static int init_connection(void);
static void end_connection(int sock);
static int read_client(SOCKET sock, char *buffer);
static void write_client(SOCKET sock, const char *buffer);
static void send_message_to_all_clients(Client *clients, Client client, int actual, const char *buffer, char from_server);
static void send_message_to_all_clients_in_group(Client *clients, Client sender, int actual, const char *buffer, char from_server, int group);
static void send_private_message_to_client(Client dest, Client sender, const char *buffer);
static void add_message_to_group_historic(Client sender, const char *buffer, char from_server, int group);
static void send_group_historic_to_client(Client client, int group);
static void remove_client(Client *clients, int to_remove, int *actual);
static void clear_clients(Client *clients, int actual);
static enum COMMAND resolve_command(const char *buffer);
static bool starts_with(const char *buffer, const char *substr);

#endif /* guard */
