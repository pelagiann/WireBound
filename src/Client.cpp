#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <fstream>
#include <vector>
#include <string>

#include "protocol.h"
#include "chunk_source.hpp"

#pragma comment(lib, "Ws2_32.lib")

using namespace std;
using namespace maxxcast;

bool recvAll(SOCKET socket, char* buffer, int totalBytes)
{
    int receivedBytes = 0;

    while (receivedBytes < totalBytes)
    {
        int received = recv(socket,
                            buffer + receivedBytes,
                            totalBytes - receivedBytes,
                            0);

        if (received <= 0)
        {
            return false;
        }

        receivedBytes += received;
    }

    return true;
}

int main()
{
    const int PORT = 6767;

    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cout << "WSAStartup failed\n";
        return 1;
    }

    SOCKET clientSocket =
        socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (clientSocket == INVALID_SOCKET)
    {
        cout << "Socket creation failed\n";

        WSACleanup();

        return 1;
    }

    sockaddr_in serverAddr{};

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);

    InetPton(AF_INET,
             TEXT("127.0.0.1"),
             &serverAddr.sin_addr.s_addr);

    if (connect(clientSocket,
                (sockaddr*)&serverAddr,
                sizeof(serverAddr)) == SOCKET_ERROR)
    {
        cout << "Connection failed\n";

        closesocket(clientSocket);

        WSACleanup();

        return 1;
    }

    cout << "Connected to server\n";
    
    BOOL flag = TRUE;

	setsockopt(clientSocket,
           IPPROTO_TCP,
           TCP_NODELAY,
           (char*)&flag,
           sizeof(flag));
           
    int recvbuf = 1024 * 1024;

	setsockopt(clientSocket,
           SOL_SOCKET,
           SO_RCVBUF,
           (char*)&recvbuf,
           sizeof(recvbuf));

    FileMetaWire wireMeta;

    bool metaOk =
        recvAll(clientSocket,
                (char*)&wireMeta,
                sizeof(wireMeta));

    if (!metaOk)
    {
        cout << "Failed to receive metadata\n";

        closesocket(clientSocket);

        WSACleanup();

        return 1;
    }

    uint32_t filenameLen = 0;

    bool filenameLenOk =
        recvAll(clientSocket,
                (char*)&filenameLen,
                sizeof(filenameLen));

    if (!filenameLenOk)
    {
        cout << "Failed to receive filename length\n";

        closesocket(clientSocket);

        WSACleanup();

        return 1;
    }

    string filename;
    filename.resize(filenameLen);

    bool filenameOk =
        recvAll(clientSocket,
                filename.data(),
                filenameLen);

    if (!filenameOk)
    {
        cout << "Failed to receive filename\n";

        closesocket(clientSocket);

        WSACleanup();

        return 1;
    }

    FileMeta meta;
    meta.filename = filename;
    meta.total_size = wireMeta.total_size;
    meta.chunk_size = wireMeta.chunk_size;
    meta.chunk_count = wireMeta.chunk_count;

    cout << "Receiving file: "
         << meta.filename
         << endl;

    cout << "File size: "
         << meta.total_size
         << " bytes\n";

    cout << "Chunk size: "
         << meta.chunk_size
         << endl;

    cout << "Chunk count: "
         << meta.chunk_count
         << endl;

    ofstream outputFile(meta.filename,
                        ios::binary);

    if (!outputFile.is_open())
    {
        cout << "Failed to create output file\n";

        closesocket(clientSocket);

        WSACleanup();

        return 1;
    }

    while (true)
    {
        ChunkHeader header;

        bool headerOk =
            recvAll(clientSocket,
                    (char*)&header,
                    sizeof(header));

        if (!headerOk)
        {
            cout << "Transfer complete or connection closed\n";
            break;
        }

        vector<char> buffer(header.size);

        bool chunkOk =
            recvAll(clientSocket,
                    buffer.data(),
                    header.size);

        if (!chunkOk)
        {
            cout << "Chunk receive failed\n";
            break;
        }

        outputFile.write(buffer.data(),
                         header.size);

        cout << "Received chunk "
             << header.index
             << " ("
             << header.size
             << " bytes)\n";
    }

    outputFile.close();

    closesocket(clientSocket);

    WSACleanup();

    cout << "File reconstruction complete\n";

    return 0;
}