#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include "server.h"
#include "client.h"

static void init(void)
{
#ifdef WIN32
   WSADATA wsa;
   int err = WSAStartup(MAKEWORD(2, 2), &wsa);
   if (err < 0)
   {
      puts("WSAStartup failed !");
      exit(EXIT_FAILURE);
   }
#endif
}

static void end(void)
{
#ifdef WIN32
   WSACleanup();
#endif
}

static void app(void)
{
   SOCKET sock = init_connection();
   char buffer[BUF_SIZE];
   /* the index for the array */
   int actual = 0;
   int max = sock;
   /* an array for all clients */
   Client clients[MAX_CLIENTS];

   fd_set rdfs;

   while (1)
   {
      int i = 0;
      FD_ZERO(&rdfs);

      /* add STDIN_FILENO */
      FD_SET(STDIN_FILENO, &rdfs);

      /* add the connection socket */
      FD_SET(sock, &rdfs);

      /* add socket of each client */
      for (i = 0; i < actual; i++)
      {
         FD_SET(clients[i].sock, &rdfs);
      }

      if (select(max + 1, &rdfs, NULL, NULL, NULL) == -1)
      {
         perror("select()");
         exit(errno);
      }

      /* something from standard input : i.e keyboard */
      if (FD_ISSET(STDIN_FILENO, &rdfs))
      {
         /* stop process when type on keyboard */
         break;
      }
      else if (FD_ISSET(sock, &rdfs))
      {
         /* new client */
         SOCKADDR_IN csin = {0};
         size_t sinsize = sizeof csin;
         int csock = accept(sock, (SOCKADDR *)&csin, &sinsize);
         if (csock == SOCKET_ERROR)
         {
            perror("accept()");
            continue;
         }

         /* after connecting the client sends its name */
         if (read_client(csock, buffer) == -1)
         {
            /* disconnected */
            continue;
         }

         /* what is the new maximum fd ? */
         max = csock > max ? csock : max;

         FD_SET(csock, &rdfs);

         /* Check if name is unique */
         Client c = {csock};
         strncpy(c.name, buffer, BUF_SIZE - 1);
         int notPresent = 0;
         for (int i = 0; i < actual; i++)
         {
            if (strcmp(clients[i].name, c.name) == 0)
            {
               write_client(c.sock, "This name is already taken, reconnect with another name");
               notPresent = 1;
               break;
            }
         }
         if (notPresent == 0)
         {
            clients[actual] = c;
            actual++;
         }
         else
         {
            closesocket(c.sock);
         }

         /* Add new client to group 0 (AKA main group) */
         clients[actual].groupId = 0;
      }
      else
      {
         int i = 0;
         for (i = 0; i < actual; i++)
         {
            /* a client is talking */
            if (FD_ISSET(clients[i].sock, &rdfs))
            {
               Client client = clients[i];
               int c = read_client(clients[i].sock, buffer);
               /* client disconnected */
               if (c == 0)
               {
                  closesocket(clients[i].sock);
                  remove_client(clients, i, &actual);
                  strncpy(buffer, client.name, BUF_SIZE - 1);
                  strncat(buffer, " disconnected !", BUF_SIZE - strlen(buffer) - 1);
                  send_message_to_all_clients_in_group(clients, client, actual, buffer, 1, client.groupId);
               }
               else
               {
                  /* Check if it's a command */
                  enum COMMAND cmd = resolve_command(buffer);
                  int j;
                  switch (cmd)
                  {
                  case NO_COMMAND:
                     send_message_to_all_clients_in_group(clients, client, actual, buffer, 0, client.groupId);
                     add_message_to_group_historic(client, buffer, 0, client.groupId);
                     break;
                  case PRIVATE_MSG_COMMAND:;
                     /* Get destination client name*/
                     char name[BUF_SIZE];
                     j = 3;
                     while (buffer[j] != ' ' && j < BUF_SIZE)
                     {
                        name[j - 3] = buffer[j];
                        j++;
                     }
                     name[j - 3] = '\0';

                     /* Find client from name */
                     Client *dest = NULL;
                     for (j = 0; j < actual; j++)
                     {
                        if (strcmp(clients[j].name, name) == 0)
                        {
                           dest = &clients[j];
                        }
                     }

                     if (dest == NULL)
                     {
                        write_client(client.sock, "Destination user not found");
                     }
                     else
                     {
                        /* Create a new buffer without the command and the name */
                        memmove(buffer, buffer + 4 + strlen(name), strlen(buffer));

                        /* Send the private message */
                        send_private_message_to_client(*dest, client, buffer);
                     }
                     break;
                  case CHANGE_GROUP_COMMAND:;
                     /* Get destination group name*/
                     char group_name[BUF_SIZE];
                     j = 6;
                     while (buffer[j] != ' ' && j < BUF_SIZE)
                     {
                        group_name[j - 6] = buffer[j];
                        j++;
                     }
                     name[j - 3] = '\0';

                     /* Alert people in the room that client leaved */
                     char msg[BUF_SIZE];
                     strncpy(msg, client.name, BUF_SIZE - 1);
                     strncat(msg, " leaved the room", sizeof msg - strlen(msg) - 1);
                     send_message_to_all_clients_in_group(clients, client, actual, msg, 1, clients[i].groupId);

                     /* WIP: Only numbers for now */
                     clients[i].groupId = atoi(group_name);

                     /* Send success message to client */
                     strncpy(msg, "Joined chatroom ", BUF_SIZE - 1);
                     strncat(msg, group_name, sizeof msg - strlen(msg) - 1);
                     strncat(msg, "\n", sizeof msg - strlen(msg) - 1);
                     write_client(client.sock, msg);

                     /* Show historic of the channel to the client */
                     strncpy(msg, "------Historic of the room------", BUF_SIZE - 1);
                     write_client(client.sock, msg);
                     send_group_historic_to_client(client, clients[i].groupId);
                     strncpy(msg, "--------End of historic---------", BUF_SIZE - 1);
                     write_client(client.sock, msg);

                     /* Alert people in the new group that client joined  */
                     strncpy(msg, client.name, BUF_SIZE - 1);
                     strncat(msg, " joined the room\n", sizeof msg - strlen(msg) - 1);
                     send_message_to_all_clients_in_group(clients, client, actual, msg, 1, clients[i].groupId);

                     break;
                  case UNKNOWN_COMMAND:
                     write_client(client.sock, "Unknown command");
                  default:
                     break;
                  }
               }
               break;
            }
         }
      }
   }

   clear_clients(clients, actual);
   end_connection(sock);
}

static void clear_clients(Client *clients, int actual)
{
   int i = 0;
   for (i = 0; i < actual; i++)
   {
      closesocket(clients[i].sock);
   }
}

static void remove_client(Client *clients, int to_remove, int *actual)
{
   /* we remove the client in the array */
   memmove(clients + to_remove, clients + to_remove + 1, (*actual - to_remove - 1) * sizeof(Client));
   /* number client - 1 */
   (*actual)--;
}

/* Send a message to every user in a group */
static void send_message_to_all_clients_in_group(Client *clients, Client sender, int actual, const char *buffer, char from_server, int group)
{
   int i = 0;
   char message[BUF_SIZE];
   message[0] = 0;

   char groupnb[10];
   sprintf(groupnb, "%d", group);
   if (group != 0)
   {
      strncpy(message, "[Group ", BUF_SIZE - 1);
      strncat(message, groupnb, sizeof message - strlen(message) - 1);
      strncat(message, "] ", sizeof message - strlen(message) - 1);
   }
   else
   {
      strncpy(message, "[General] ", BUF_SIZE - 1);
   }

   if (from_server == 0)
   {
      strncat(message, sender.name, sizeof message - strlen(message) - 1);
      strncat(message, " : ", sizeof message - strlen(message) - 1);
   }
   strncat(message, buffer, sizeof message - strlen(message) - 1);

   for (i = 0; i < actual; i++)
   {
      if (group == clients[i].groupId)
      {
         /* we don't send message to the sender */
         if (sender.sock != clients[i].sock)
         {
            write_client(clients[i].sock, message);
         }
      }
   }
}

static void send_message_to_all_clients(Client *clients, Client sender, int actual, const char *buffer, char from_server)
{
   int i = 0;
   char message[BUF_SIZE];
   message[0] = 0;
   if (from_server == 0)
   {
      strncpy(message, sender.name, BUF_SIZE - 1);
      strncat(message, " : ", sizeof message - strlen(message) - 1);
   }
   strncat(message, buffer, sizeof message - strlen(message) - 1);

   for (i = 0; i < actual; i++)
   {
      /* we don't send message to the sender */
      if (sender.sock != clients[i].sock)
      {
         write_client(clients[i].sock, message);
      }
   }
}

static void send_private_message_to_client(Client dest, Client sender, const char *buffer)
{
   int i = 0;
   char message[BUF_SIZE];
   message[0] = 0;

   strncpy(message, "De [", BUF_SIZE - 1);
   strncat(message, sender.name, sizeof message - strlen(message) - 1);
   strncat(message, "] : ", sizeof message - strlen(message) - 1);

   strncat(message, buffer, sizeof message - strlen(message) - 1);
   write_client(dest.sock, message);
}

static enum COMMAND resolve_command(const char *buffer)
{
   /* check if it's a command */
   if (buffer[0] == '/')
   {
      if (starts_with(buffer, "/m "))
      {
         return PRIVATE_MSG_COMMAND;
      }
      else if (starts_with(buffer, "/join"))
      {
         return CHANGE_GROUP_COMMAND;
      }
      else
      {
         return UNKNOWN_COMMAND;
      }
   }
   else
   {
      return NO_COMMAND;
   }
}

static bool starts_with(const char *buffer, const char *substr)
{
   bool startsWith = false;
   for (int i = 0; substr[i] != '\0'; i++)
   {
      if (buffer[i] != substr[i])
      {
         startsWith = false;
         break;
      }
      startsWith = true;
   }
   return startsWith;
}

static void add_message_to_group_historic(Client sender, const char *buffer, char from_server, int group)
{
   char message[BUF_SIZE];
   message[0] = 0;

   char groupnb[10];
   sprintf(groupnb, "%d", group);
   if (group != 0)
   {
      strncpy(message, "[Group ", BUF_SIZE - 1);
      strncat(message, groupnb, sizeof message - strlen(message) - 1);
      strncat(message, "] ", sizeof message - strlen(message) - 1);
   }
   else
   {
      strncpy(message, "[General] ", BUF_SIZE - 1);
   }

   if (from_server == 0)
   {
      strncat(message, sender.name, sizeof message - strlen(message) - 1);
      strncat(message, " : ", sizeof message - strlen(message) - 1);
   }
   strncat(message, buffer, sizeof message - strlen(message) - 1);

   if (chdir("../Historique") == 0)
   {
      FILE *file;
      char groupnb[15];
      sprintf(groupnb, "%d", group);

      file = fopen(strcat(groupnb, ".txt"), "a");
      fprintf(file, "%s", strcat(message, "\n"));
      fclose(file);
   }
}

static void send_group_historic_to_client(Client client, int group)
{
   if (chdir("../Historique") == 0)
   {
      FILE *file;
      char groupnb[15];
      sprintf(groupnb, "%d", group);
      file = fopen(strcat(groupnb, ".txt"), "r");
      char buffer[BUF_SIZE];
      if (file != NULL)
      {
         while (fgets(buffer, BUF_SIZE, file))
         {
            write_client(client.sock, buffer);
         }
         fclose(file);
      }
   }
}

static int init_connection(void)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = {0};

   if (sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   sin.sin_port = htons(PORT);
   sin.sin_family = AF_INET;

   if (bind(sock, (SOCKADDR *)&sin, sizeof sin) == SOCKET_ERROR)
   {
      perror("bind()");
      exit(errno);
   }

   if (listen(sock, MAX_CLIENTS) == SOCKET_ERROR)
   {
      perror("listen()");
      exit(errno);
   }

   return sock;
}

static void end_connection(int sock)
{
   closesocket(sock);
}

static int read_client(SOCKET sock, char *buffer)
{
   int n = 0;

   if ((n = recv(sock, buffer, BUF_SIZE - 1, 0)) < 0)
   {
      perror("recv()");
      /* if recv error we disonnect the client */
      n = 0;
   }

   buffer[n] = 0;

   return n;
}

static void write_client(SOCKET sock, const char *buffer)
{
   if (send(sock, buffer, strlen(buffer), 0) < 0)
   {
      perror("send()");
      exit(errno);
   }
}

int main(int argc, char **argv)
{
   init();

   app();

   end();

   return EXIT_SUCCESS;
}
