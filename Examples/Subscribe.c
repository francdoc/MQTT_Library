// run on terminal -> $./Subscribe test.mosquitto.org 1883

/** @file Subscribe.c
 * Topic "subscribe" example and test case for the MQTT library.
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

//-------------------------------------------------------------------------------------------------
// Entry point
//-------------------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    printf("Running subscribe sample program.\n");
    static unsigned char Buffer[1024]; // Avoid storing a big buffer on the stack
    char *Pointer_String_Server_IP_Address;
    unsigned short Server_Port;
    int Socket, Result;
    TMQTTContext MQTT_Context;
    TMQTTConnectionParameters MQTT_Connection_Parameters;
    ssize_t Read_Bytes_Count;

    // Check parameters
    if (argc != 3)
    {
        printf("Usage : %s MQTT_Server_IP_Address MQTT_Server_Port\n"
               "Use wireshark or another network traffic analyzer in the same time to check if the packets are well formed.\n",
               argv[0]);
        return EXIT_FAILURE;
    }

	///////////////////////////////////////////////////////////////////////////
    Pointer_String_Server_IP_Address = argv[1];
    Server_Port = atoi(argv[2]);

    printf("MQTT Server IP Address: %s\n", Pointer_String_Server_IP_Address);
    printf("MQTT Server Port: %d\n", Server_Port);

    // Convert Server_Port to string
    char Port_String[6];
    snprintf(Port_String, sizeof(Port_String), "%u", Server_Port);

    printf("Opening socket...\n");
    /* open the non-blocking TCP socket (connecting to the broker) */
    Socket = open_nb_socket(Pointer_String_Server_IP_Address, Port_String);

    if (Socket == -1)
    {
        perror("Failed to open socket: ");
        return EXIT_FAILURE;
    }
	///////////////////////////////////////////////////////////////////////////

	// Send MQTT CONNECT packet
    printf("Sending CONNECT packet...\n");
    MQTT_Connection_Parameters.Pointer_String_Client_Identifier = "NULL";
    MQTT_Connection_Parameters.Pointer_String_User_Name = NULL;
    MQTT_Connection_Parameters.Pointer_String_Password = NULL;
    MQTT_Connection_Parameters.Is_Clean_Session_Enabled = 1;
    MQTT_Connection_Parameters.Keep_Alive = 60;
    MQTT_Connection_Parameters.Pointer_Buffer = Buffer;
    MQTTConnect(&MQTT_Context, &MQTT_Connection_Parameters);
    if (write(Socket, MQTT_GET_MESSAGE_BUFFER(&MQTT_Context), MQTT_GET_MESSAGE_SIZE(&MQTT_Context)) != MQTT_GET_MESSAGE_SIZE(&MQTT_Context)) {
        printf("Error : failed to send MQTT CONNECT packet (%s).\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // Specifications allow to send control packets without waiting for CONNACK, but if the client is too fast the server can't keep up, so wait for the CONNACK
    printf("Waiting for CONNACK packet...\n");
    Read_Bytes_Count = read_socket(Socket, Buffer, MQTT_CONNACK_MESSAGE_SIZE);
    if (Read_Bytes_Count < 0) {
        printf("Error : failed to read CONNACK packet.\n");
        return EXIT_FAILURE;
    }

    Result = MQTTIsConnectionEstablished(Buffer, Read_Bytes_Count);
    if (Result != 0) {
        printf("Error : server rejected connection. CONNACK return code : 0x%X\n", Result);
        return EXIT_FAILURE;
    }

    // Subscribe to a topic
    printf("Sending SUBSCRIBE packet...\n");
    MQTTSubscribe(&MQTT_Context, "test/topic");
    if (write(Socket, MQTT_GET_MESSAGE_BUFFER(&MQTT_Context), MQTT_GET_MESSAGE_SIZE(&MQTT_Context)) != MQTT_GET_MESSAGE_SIZE(&MQTT_Context))
    {
        printf("Error : failed to send MQTT SUBSCRIBE packet (%s).\n", strerror(errno));
        return EXIT_FAILURE;
    }

	printf("Press to send disconnect packet.\n");

	getchar();

	// Disconnect from MQTT server
	MQTTDisconnect(&MQTT_Context);
	printf("Sending DISCONNECT packet...\n");
	if (write(Socket, MQTT_GET_MESSAGE_BUFFER(&MQTT_Context), MQTT_GET_MESSAGE_SIZE(&MQTT_Context)) != MQTT_GET_MESSAGE_SIZE(&MQTT_Context))
    {
        printf("Error : failed to send MQTT DISCONNECT packet (%s).\n", strerror(errno));
        return EXIT_FAILURE;
    }
    close(Socket);

    return 0;
}
