/*
	An example chatbot that will connect to a local Wesnoth server with the username "ChatBot" and respond to the "\wave" command. 
	
	The majority of the code is based off the Winsock example provided by Microsoft: https://docs.microsoft.com/en-us/windows/win32/winsock/complete-client-code
	
	The packet data and the approach to reverse it are discussed in the article at: https://gamehacking.academy/lesson/33
*/

#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#include <zlib.h>

#define DEFAULT_BUFLEN 512

void send_data(const unsigned char *data, size_t len, SOCKET s) {
    gzFile temp_data = gzopen("packet.gz", "wb");
    gzwrite(temp_data, data, len);
    gzclose(temp_data);

    FILE* temp_file = NULL;
    fopen_s(&temp_file, "packet.gz", "rb");

    if (temp_file) {
        size_t compress_len = 0;
        unsigned char buffer[DEFAULT_BUFLEN] = { 0 };
        compress_len = fread(buffer, 1, sizeof(buffer), temp_file);
        fclose(temp_file);

        unsigned char buff_packet[DEFAULT_BUFLEN] = { 0 };
        memcpy(buff_packet + 3, &compress_len, sizeof(compress_len));
        memcpy(buff_packet + 4, buffer, compress_len);

        int iResult = send(s, (const char*)buff_packet, compress_len + 4, 0);
        printf("Bytes Sent: %ld\n", iResult);
    }
}

bool parse_data(unsigned char *buff, int buff_len) {
    unsigned char data[DEFAULT_BUFLEN] = { 0 };
    memcpy(data, buff + 4, buff_len - 4);
    
    FILE* temp_file = NULL;
    fopen_s(&temp_file, "packet_recv.gz", "wb");

    if (temp_file) {
        fwrite(data, 1, sizeof(data), temp_file);
        fclose(temp_file);
    }
    
    gzFile temp_data_in = gzopen("packet_recv.gz", "rb");
    unsigned char decompressed_data[DEFAULT_BUFLEN] = { 0 };
    gzread(temp_data_in, decompressed_data, DEFAULT_BUFLEN);
    fwrite(decompressed_data, 1, DEFAULT_BUFLEN, stdout);
    gzclose(temp_data_in);

    return strstr((const char*)decompressed_data, (const char*)"\\wave");
}

int main(int argc, char** argv) {
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;
    unsigned char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;

    // The handshake initiation request
    const unsigned char buff_handshake_p1[] = {
        0x00, 0x00, 0x00, 0x00
    };

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo("127.0.0.1", "15000", &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    ptr = result;

    ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Error at socket(): %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        closesocket(ConnectSocket);
        ConnectSocket = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }

    iResult = send(ConnectSocket, (const char*)buff_handshake_p1, (int)sizeof(buff_handshake_p1), 0);
    printf("Bytes Sent: %ld\n", iResult);

    iResult = recv(ConnectSocket, (char*)recvbuf, recvbuflen, 0);
    printf("Bytes received: %d\n", iResult);

    const unsigned char version[] = "[version]\nversion=\"1.14.9\"\n[/version]";
    send_data(version, sizeof(version), ConnectSocket);

    iResult = recv(ConnectSocket, (char*)recvbuf, recvbuflen, 0);
    printf("Bytes received: %d\n", iResult);

    const unsigned char name[] = "[login]\nusername=\"ChatBot\"\n[/login]";
    send_data(name, sizeof(name), ConnectSocket);

    const unsigned char first_message[] = "[message]\nmessage=\"ChatBot connected\"\nroom=\"lobby\"\nsender=\"ChatBot\"\n[/message]";
    send_data(first_message, sizeof(first_message), ConnectSocket);

    do {
        iResult = recv(ConnectSocket, (char*)recvbuf, recvbuflen, 0);
        if (iResult > 0)
            printf("Bytes received: %d\n", iResult);
        else if (iResult == 0)
            printf("Connection closed\n");
        else
            printf("recv failed with error: %d\n", WSAGetLastError());

        if (parse_data(recvbuf, iResult)) {
            const unsigned char message[] = "[message]\nmessage=\"Hello!\"\nroom=\"lobby\"\nsender=\"ChatBot\"\n[/message]";
            send_data(message, sizeof(message), ConnectSocket);
        }
    } while (iResult > 0);

    closesocket(ConnectSocket);
    WSACleanup();

    return 0;
}
