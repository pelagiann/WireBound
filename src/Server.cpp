#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <fstream>
#include <filesystem>
#include <vector>
#include <chrono>

#include "protocol.h"
#include "chunk_source.hpp"

#pragma comment(lib, "Ws2_32.lib")

using namespace std;
using namespace maxxcast;

bool sendAll(SOCKET socket, const char* data, int totalBytes)
{
    int sentBytes = 0;

    while (sentBytes < totalBytes)
    {
        int sent = send(socket,
                        data + sentBytes,
                        totalBytes - sentBytes,
                        0);

        if (sent <= 0)
        {
            return false;
        }

        sentBytes += sent;
    }

    return true;
}

uint64_t get_file_size(const string& path)
{
    ifstream file(path, ios::binary | ios::ate);
    return static_cast<uint64_t>(file.tellg());
}

int main()
{
    const int PORT = 6767;
    const uint32_t CHUNK_SIZE = 256*1024;

    string filePath = "C:/Users/Ishan/Desktop/Test.pdf";

    uint64_t fileSize = get_file_size(filePath);

    uint64_t chunkCount =
        (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;

    FileMeta meta;
    meta.filename = filesystem::path(filePath).filename().string();
    meta.total_size = fileSize;
    meta.chunk_size = CHUNK_SIZE;
    meta.chunk_count = chunkCount;

    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cout << "WSAStartup failed\n";
        return 1;
    }

    SOCKET serverSocket =
        socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (serverSocket == INVALID_SOCKET)
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

    if (bind(serverSocket,
             (sockaddr*)&serverAddr,
             sizeof(serverAddr)) == SOCKET_ERROR)
    {
        cout << "Bind failed\n";

        closesocket(serverSocket);
        WSACleanup();

        return 1;
    }

    if (listen(serverSocket, 1) == SOCKET_ERROR)
    {
        cout << "Listen failed\n";

        closesocket(serverSocket);
        WSACleanup();

        return 1;
    }

    cout << "Listening on port "
         << PORT
         << endl;

    SOCKET clientSocket =
        accept(serverSocket, nullptr, nullptr);

    if (clientSocket == INVALID_SOCKET)
    {
        cout << "Accept failed\n";

        closesocket(serverSocket);
        WSACleanup();

        return 1;
    }

    cout << "Client connected\n";
    
    BOOL flag = TRUE;

	setsockopt(clientSocket,
           IPPROTO_TCP,
           TCP_NODELAY,
           (char*)&flag,
           sizeof(flag));
    int sndbuf = 1024 * 1024;

	setsockopt(clientSocket,
           SOL_SOCKET,
           SO_SNDBUF,
           (char*)&sndbuf,
           sizeof(sndbuf));
    FileMetaWire wireMeta;
    wireMeta.total_size = meta.total_size;
    wireMeta.chunk_size = meta.chunk_size;
    wireMeta.chunk_count = meta.chunk_count;

    if (!sendAll(clientSocket,
                 (char*)&wireMeta,
                 sizeof(wireMeta)))
    {
        cout << "Failed to send metadata\n";
        return 1;
    }

    uint32_t filenameLen =
        static_cast<uint32_t>(meta.filename.size());

    if (!sendAll(clientSocket,
                 (char*)&filenameLen,
                 sizeof(filenameLen)))
    {
        cout << "Failed to send filename length\n";
        return 1;
    }

    if (!sendAll(clientSocket,
                 meta.filename.c_str(),
                 filenameLen))
    {
        cout << "Failed to send filename\n";
        return 1;
    }

    ifstream file(filePath, ios::binary);

    vector<char> buffer(CHUNK_SIZE);

    uint64_t chunkIndex = 0;
    
    auto start = chrono::high_resolution_clock::now();

    while (true)
    {
        file.read(buffer.data(), buffer.size());

        streamsize bytesRead = file.gcount();

        if (bytesRead <= 0)
        {
            break;
        }

        ChunkHeader header;
        header.index = chunkIndex++;
        header.size = static_cast<uint32_t>(bytesRead);

        bool headerOk =
            sendAll(clientSocket,
                    (char*)&header,
                    sizeof(header));

        bool chunkOk =
            sendAll(clientSocket,
                    buffer.data(),
                    static_cast<int>(bytesRead));

        if (!headerOk || !chunkOk)
        {
            cout << "Transfer failed\n";
            break;
        }

        cout << "Sent chunk "
             << header.index
             << " ("
             << header.size
             << " bytes)\n";
    }

    cout << "Transfer complete\n";

    file.close();
    
    auto end =
    chrono::high_resolution_clock::now();

	double seconds =
    chrono::duration<double>(end - start).count();

	double mbps =
    (fileSize / (1024.0 * 1024.0)) / seconds;

	cout << "\nTransfer speed: "
     << mbps
     << " MB/s\n";

    closesocket(clientSocket);
    closesocket(serverSocket);

    WSACleanup();

    return 0;
}