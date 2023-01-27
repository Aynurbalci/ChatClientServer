#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#define PORT 9013
#define NAME_LENGTH 32
#define MESSAGE_LENGTH 128
#define BUFFER_LENGTH 256
#define MAX_NUM_OF_CLIENTS 10

typedef struct clientInfo{
    int id;
    struct sockaddr_in address;
    char name[NAME_LENGTH];
    int socketId;
} clientInfo;

typedef struct MESG { //mesaj komutu
    char command[5];
    char message[MESSAGE_LENGTH];
    char crc[33];
    char from[NAME_LENGTH];
    char to[NAME_LENGTH];
    char parity;

} MESG;

typedef struct MERR { //hatalı mesaj komutu
    char command[5];
    char from[NAME_LENGTH];
    char to[NAME_LENGTH];

} MERR;

typedef struct CONN { //baglantı komutu
    char command[5];
    char name[NAME_LENGTH];
} CONN;

void message_to_string(MESG* msg, char buffer[]){
    sprintf(buffer, "%s|%s|%s|%s|%s|%c", msg->command, msg->to, msg->from, msg->message, msg->crc, msg->parity);
}
void connection_to_string(CONN* connection, char buffer[]){
    sprintf(buffer, "%s|%s", connection->command, connection->name);
}

void message_error_to_string(MERR* msg, char buffer[]){
    sprintf(buffer, "%s|%s|%s", msg->command, msg->to, msg->from);
}

void string_to_message(char buffer[], MESG* msg){
    char copy[BUFFER_LENGTH];
    strcpy(copy, buffer);
    strcpy(msg->command,strtok(copy, "|"));
    strcpy(msg->to, strtok(NULL, "|"));
    strcpy(msg->from, strtok(NULL, "|"));
    strcpy(msg->message, strtok(NULL, "|"));
    strcpy(msg->crc, strtok(NULL, "|"));
    msg->parity = strtok(NULL, "|")[0];
}

void string_to_connection(char buffer[], CONN* connection){
    char copy[BUFFER_LENGTH];
    strcpy(copy, buffer);
    strcpy(connection->command,strtok(copy, "|"));
    strcpy(connection->name, strtok(NULL, "|"));
}

void string_to_message_error(char buffer[], MERR* msg){
    char copy[BUFFER_LENGTH];
    strcpy(copy, buffer);
    strcpy(msg->command,strtok(copy, "|"));
    strcpy(msg->to, strtok(NULL, "|"));
    strcpy(msg->from, strtok(NULL, "|"));
}


