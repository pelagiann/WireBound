#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>

#include "protocol.h"
#include "chunk_source.hpp"
#include "picosha2.h"

#pragma comment(lib, "Ws2_32.lib")

using namespace std;
using namespace maxxcast;

// recvAll — loop until every expected byte has arrived.
bool recvAll(SOCKET sock, void* buf, int len)
{
    char* ptr = static_cast<char*>(buf);
    int received = 0;
    while (received < len)
    {
        int n = recv(sock, ptr + received, len - received, 0);
        if (n <= 0) return false;
        received += n;
    }
    return true;
}

int main()
{
    const int PORT = 6767;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        cerr << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port   = htons(PORT);
    InetPton(AF_INET, TEXT("127.0.0.1"), &serverAddr.sin_addr);

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        cerr << "Connection failed (error " << WSAGetLastError() << ")\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    cout << "Connected to server\n";

    // Tune socket for bulk receive
    BOOL nodelay = TRUE;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));
    int recvbuf = 4 * 1024 * 1024;   // 4 MB receive buffer (matches server's send buffer)
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&recvbuf, sizeof(recvbuf));

    // Receive handshake: FileMetaWire
    FileMetaWire wire{};
    if (!recvAll(sock, &wire, sizeof(wire)))
    {
        cerr << "Failed to receive metadata\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    string expected_hash(wire.sha256_hex, 64);

    // Receive handshake: filename
    uint32_t fnLen = 0;
    if (!recvAll(sock, &fnLen, sizeof(fnLen)))
    {
        cerr << "Failed to receive filename length\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    string filename;
    filename.resize(fnLen);
    if (!recvAll(sock, filename.data(), static_cast<int>(fnLen)))
    {
        cerr << "Failed to receive filename\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    cout << "\nReceiving : " << filename        << "\n";
    cout << "Size      : " << wire.total_size  << " bytes\n";
    cout << "Chunks    : " << wire.chunk_count
         << " x " << wire.chunk_size << " bytes\n";
    cout << "Expected SHA-256: " << expected_hash << "\n\n";

    // Open output file
    ofstream outFile(filename, ios::binary);
    if (!outFile.is_open())
    {
        cerr << "Failed to create output file: " << filename << "\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Incremental SHA-256 hasher.
    picosha2::hash256_one_by_one hasher;
    hasher.init();

    uint64_t chunks_received = 0;
    uint64_t bytes_received  = 0;

    // Receive chunk loop
    while (true)
    {
        ChunkHeader hdr{};
        if (!recvAll(sock, &hdr, sizeof(hdr)))
        {
        	cerr<<"\nConnection lost unexpectedly\n";
        	outFile.close();
        	closesocket(sock);
        	WSACleanup();
        	return 1;
            
        }
        if(hdr.type==PACKET_END)
        {
        	cout<<"\nReceived END packet\n";
        	break;
        }
        if (hdr.type != PACKET_CHUNK)
    	{
        	cerr << "\nUnknown packet type: "
             << hdr.type << "\n";

        	outFile.close();
        	closesocket(sock);
        	WSACleanup();

       	    return 1;
    	}

        vector<char> buf(hdr.size);
        if (!recvAll(sock, buf.data(), static_cast<int>(hdr.size)))
        {
            cerr << "\nFailed to receive chunk " << hdr.index << "\n";
            outFile.close();
            closesocket(sock);
            WSACleanup();
            return 1;
        }

        // Write to disk
        outFile.write(buf.data(), hdr.size);

        // Feed into the incremental hasher
        hasher.process(buf.begin(), buf.end());

        chunks_received++;
        bytes_received += hdr.size;

        // Progress
        double pct = wire.chunk_count > 0
            ? (double)chunks_received / wire.chunk_count * 100.0
            : 0.0;
        printf("\r  chunk %llu/%llu  (%.1f%%)  %llu bytes",
               (unsigned long long)chunks_received,
               (unsigned long long)wire.chunk_count,
               pct,
               (unsigned long long)bytes_received);
        fflush(stdout);
    }

    outFile.close();
    hasher.finish();

    // SHA-256 verification
    string computed_hash = picosha2::get_hash_hex_string(hasher);

    cout << "\n\nComputed  SHA-256: " << computed_hash  << "\n";
    cout << "Expected  SHA-256: " << expected_hash << "\n";

    if (computed_hash == expected_hash)
    {
        cout << "\nRESULT: PASS — file received correctly. Hashes match.\n";
        cout << "Saved as: " << filename << "\n";
    }
    else
    {
        cout << "\nRESULT: FAIL — SHA-256 mismatch. File is corrupted.\n";
        closesocket(sock);
        WSACleanup();
        system("pause");
        return 1;
    }

    closesocket(sock);
    WSACleanup();
    system("pause");
    return 0;
}
