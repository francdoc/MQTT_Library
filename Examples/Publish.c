// run on terminal -> $./Publish test.mosquitto.org 1883

/** @file Publish.c
 * Topic "publish" example and test case for the MQTT library.
 * @author Adrien RICCIARDI
 */
#include <arpa/inet.h>
#include <errno.h>
#include <MQTT.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>

#include "posix_sockets.h"

//-------------------------------------------------------------------------------------------------
// Private variables
//-------------------------------------------------------------------------------------------------
/** Server IP address. */
static char Publish_String_Server_IP_Address[40]; // Enough for an IPv6 address.
/** Server port. */
static unsigned short Publish_Server_Port;

//-------------------------------------------------------------------------------------------------
// Private functions
//-------------------------------------------------------------------------------------------------
ssize_t read_socket(int socket, unsigned char *buffer, size_t size) {
    size_t total_read = 0;
    ssize_t bytes_read;

    while (total_read < size) {
        fd_set read_fds;
        struct timeval timeout;

        FD_ZERO(&read_fds);
        FD_SET(socket, &read_fds);
        timeout.tv_sec = 5; // Timeout after 5 seconds
        timeout.tv_usec = 0;

        int select_result = select(socket + 1, &read_fds, NULL, NULL, &timeout);
        if (select_result < 0) {
            perror("Select error");
            return -1;
        } else if (select_result == 0) {
            fprintf(stderr, "Read timeout\n");
            return -1;
        }

        bytes_read = read(socket, buffer + total_read, size - total_read);
        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                perror("Read error");
                return -1;
            }
        } else if (bytes_read == 0) {
            break;
        }

        total_read += bytes_read;
        printf("Read %zd bytes, total_read: %zu\n", bytes_read, total_read);
    }

    return total_read;
}

void PublishConnectAndPublishData(char *Pointer_String_Client_ID, char *Pointer_String_User_Name, char *Pointer_String_Password, char *Pointer_String_Topic_Name, void *Pointer_Application_Data, int Application_Data_Size) {
    static unsigned char Buffer[1024]; // Avoid storing a big buffer on the stack
    TMQTTContext MQTT_Context;
    TMQTTConnectionParameters MQTT_Connection_Parameters;
    int Socket, Result;
    ssize_t Read_Bytes_Count;
    
    // Open non-blocking TCP socket
    printf("Opening socket...\n");
    char Port_String[6];
    snprintf(Port_String, sizeof(Port_String), "%u", Publish_Server_Port);
    Socket = open_nb_socket(Publish_String_Server_IP_Address, Port_String);

    if (Socket == -1) {
        perror("Failed to open socket: ");
        exit(EXIT_FAILURE);
    }
    
    // Send MQTT CONNECT packet
    printf("Sending CONNECT packet...\n");
    MQTT_Connection_Parameters.Pointer_String_Client_Identifier = Pointer_String_Client_ID;
    MQTT_Connection_Parameters.Pointer_String_User_Name = Pointer_String_User_Name;
    MQTT_Connection_Parameters.Pointer_String_Password = Pointer_String_Password;
    MQTT_Connection_Parameters.Is_Clean_Session_Enabled = 1;
    MQTT_Connection_Parameters.Keep_Alive = 60;
    MQTT_Connection_Parameters.Pointer_Buffer = Buffer;
    MQTTConnect(&MQTT_Context, &MQTT_Connection_Parameters);
    if (write(Socket, MQTT_GET_MESSAGE_BUFFER(&MQTT_Context), MQTT_GET_MESSAGE_SIZE(&MQTT_Context)) != MQTT_GET_MESSAGE_SIZE(&MQTT_Context)) {
        printf("Error : failed to send MQTT CONNECT packet (%s).\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Wait for CONNACK packet
    printf("Waiting for CONNACK packet...\n");
    Read_Bytes_Count = read_socket(Socket, Buffer, MQTT_CONNACK_MESSAGE_SIZE);
    if (Read_Bytes_Count < 0) {
        printf("Error : failed to read CONNACK packet.\n");
        exit(EXIT_FAILURE);
    }

    Result = MQTTIsConnectionEstablished(Buffer, Read_Bytes_Count);
    if (Result != 0) {
        printf("Error : server rejected connection. CONNACK return code : 0x%X\n", Result);
        printf("Client ID: %s\n", Pointer_String_Client_ID);
        printf("Username: %s\n", Pointer_String_User_Name ? Pointer_String_User_Name : "NULL");
        printf("Password: %s\n", Pointer_String_Password ? Pointer_String_Password : "NULL");
        exit(EXIT_FAILURE);
    }
    
    // Publish data
    printf("Sending PUBLISH packet...\n");
    MQTTPublish(&MQTT_Context, Pointer_String_Topic_Name, Pointer_Application_Data, Application_Data_Size);
    if (write(Socket, MQTT_GET_MESSAGE_BUFFER(&MQTT_Context), MQTT_GET_MESSAGE_SIZE(&MQTT_Context)) != MQTT_GET_MESSAGE_SIZE(&MQTT_Context)) {
        printf("Error : failed to send MQTT PUBLISH packet (%s).\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Disconnect from MQTT server
    MQTTDisconnect(&MQTT_Context);
    printf("Sending DISCONNECT packet...\n");
    if (write(Socket, MQTT_GET_MESSAGE_BUFFER(&MQTT_Context), MQTT_GET_MESSAGE_SIZE(&MQTT_Context)) != MQTT_GET_MESSAGE_SIZE(&MQTT_Context)) {
        printf("Error : failed to send MQTT DISCONNECT packet (%s).\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    putchar('\n');
    close(Socket);
}

//-------------------------------------------------------------------------------------------------
// Entry point
//-------------------------------------------------------------------------------------------------
int main(int argc, char *argv[]) {
    // Check parameters
    if (argc != 3) {
        printf("Usage : %s MQTT_Server_IP_Address MQTT_Server_Port\n"
            "Use wireshark or another network traffic analyzer in the same time to check if the packets are well formed.\n", argv[0]);
        return EXIT_FAILURE;
    }
    strcpy(Publish_String_Server_IP_Address, argv[1]);
	Publish_Server_Port = atoi(argv[2]);

	// Test message with all parameters set
	PublishConnectAndPublishData("ClientID", NULL, NULL, "test/topic", "HI MQTT!", sizeof("HI MQTT!") - 1);

	return 0;
}
