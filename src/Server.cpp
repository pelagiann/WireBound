#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <psapi.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include "protocol.h"
#include "chunk_source.hpp"
#include "mapped_chunk_source.hpp"
#include "progress_tracker.hpp"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Psapi.lib")

using namespace std;
using namespace maxxcast;

// sendAll — loop until every byte is written to the socket.
bool sendAll(SOCKET sock, const void* data, int len)
{
    const char* ptr = static_cast<const char*>(data);
    int sent = 0;
    while (sent < len)
    {
        int n = send(sock, ptr + sent, len - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

// serve_client: send the full file to one connected client.
bool serve_client(SOCKET sock, const MappedChunkSource& src, ClientProgress& progress)
{
    const FileMeta& meta = src.metadata();

    BOOL nodelay = TRUE;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));
    int sndbuf = 4 * 1024 * 1024;   // 4 MB send buffer
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&sndbuf, sizeof(sndbuf));

    FileMetaWire wire{};
    wire.total_size  = meta.total_size;
    wire.chunk_size  = meta.chunk_size;
    wire.chunk_count = meta.chunk_count;
    memcpy(wire.sha256_hex, meta.sha256_hex.c_str(), 64);

    if (!sendAll(sock, &wire, sizeof(wire)))
    {
        { lock_guard<mutex> lk(g_console_mtx);
          cerr << "\n[Client " << progress.id << "] Failed to send metadata\n"; }
        progress.failed = true;
        return false;
    }

    uint32_t fnLen = static_cast<uint32_t>(meta.filename.size());
    if (!sendAll(sock, &fnLen, sizeof(fnLen)) ||
        !sendAll(sock, meta.filename.c_str(), static_cast<int>(fnLen)))
    {
        { lock_guard<mutex> lk(g_console_mtx);
          cerr << "\n[Client " << progress.id << "] Failed to send filename\n"; }
        progress.failed = true;
        return false;
    }

    for (uint64_t i = 0; i < src.chunk_count(); ++i)
    {
        ChunkView chunk = src.get_chunk(i);

        ChunkHeader hdr{};
        hdr.type = PACKET_CHUNK;
        hdr.index = chunk.index;
        hdr.size  = static_cast<uint32_t>(chunk.len);

        if (!sendAll(sock, &hdr, sizeof(hdr)) ||
            !sendAll(sock, chunk.data, static_cast<int>(chunk.len)))
        {
            { lock_guard<mutex> lk(g_console_mtx);
              cerr << "\n[Client " << progress.id
                   << "] Transfer failed at chunk " << i << "\n"; }
            progress.failed = true;
            return false;
        }

        progress.chunks_sent++;
        progress.bytes_sent += chunk.len;
        progress.print_inline();
    }
    
    ChunkHeader endHdr{};
    endHdr.type = PACKET_END;
    
    if (!sendAll(sock, &endHdr, sizeof(endHdr)))
    {
        { lock_guard<mutex> lk(g_console_mtx);
          cerr << "\n[Client " << progress.id << "] Failed to send END packet\n"; }
        progress.failed = true;
        return false;
    }

    progress.complete = true;
    return true;
}

// Set by the Ctrl+C handler; checked in the accept loop to suppress spurious error messages.
atomic<bool> g_shutdown    { false };
SOCKET       g_serverSocket = INVALID_SOCKET;

BOOL WINAPI console_ctrl_handler(DWORD type)
{
    if (type == CTRL_C_EVENT || type == CTRL_CLOSE_EVENT)
    {
        g_shutdown = true;
        { lock_guard<mutex> lk(g_console_mtx);
          cout << "\nShutdown requested — waiting for active transfers...\n"; }
        closesocket(g_serverSocket);  // unblocks accept()
        return TRUE;
    }
    return FALSE;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        cerr << "Usage: Server.exe <file> [chunk-size-kb]\n";
        return 1;
    }

    const int PORT      = 6767;
    uint32_t  CHUNK_SIZE = 256 * 1024;
    if (argc >= 3)
        CHUNK_SIZE = static_cast<uint32_t>(stoul(argv[2])) * 1024;

    MappedChunkSource src(argv[1], CHUNK_SIZE);
    const FileMeta& meta = src.metadata();

    cout << "=== MaxxCast Server ===\n";
    cout << "File      : " << meta.filename   << "\n";
    cout << "Size      : " << meta.total_size << " bytes\n";
    cout << "Chunks    : " << meta.chunk_count
         << " x " << meta.chunk_size << " bytes\n";
    cout << "SHA-256   : " << meta.sha256_hex << "\n\n";

    IO_COUNTERS io_before{};
    GetProcessIoCounters(GetCurrentProcess(), &io_before);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET)
    {
        cerr << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    /* SO_REUSEADDR: lets us restart the server immediately without waiting
    for the OS to release the port from the previous run*/
    BOOL reuse = TRUE;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY); 

    if (bind(serverSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        cerr << "Bind failed (error " << WSAGetLastError() << ")\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        cerr << "Listen failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
    g_serverSocket = serverSocket;

    cout << "Listening on port " << PORT << " (all interfaces)...\n";

    vector<thread>    workers;
    atomic<uint64_t>  total_bytes_sent { 0 };
    atomic<int>       total_clients    { 0 };
    bool              session_started  = false;
    chrono::high_resolution_clock::time_point t_session_start;
    int client_id = 0;

    while (true)
    {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET)
        {
            if (!g_shutdown)
            { lock_guard<mutex> lk(g_console_mtx);
              cerr << "Accept failed (error " << WSAGetLastError() << ") — stopping.\n"; }
            break;
        }

        if (!session_started)
        {
            t_session_start = chrono::high_resolution_clock::now();
            session_started = true;
        }

        int id = ++client_id;

        { lock_guard<mutex> lk(g_console_mtx);
          cout << "\n[Client " << id << "] connected.\n"; }

        // Each client gets its own thread; src is read-only so sharing is safe.
        workers.emplace_back([clientSocket, id, &src, &meta,
                              &total_bytes_sent, &total_clients]()
        {
            ClientProgress progress;
            progress.id           = id;
            progress.total_chunks = meta.chunk_count;
            progress.total_bytes  = meta.total_size;

            auto t0 = chrono::high_resolution_clock::now();
            bool ok = false;
            try
            {
                ok = serve_client(clientSocket, src, progress);
            }
            catch (...)
            {
                lock_guard<mutex> lk(g_console_mtx);
                cerr << "\n[Client " << id << "] unexpected exception — dropped.\n";
            }
            auto t1 = chrono::high_resolution_clock::now();

            closesocket(clientSocket);  // always runs, even if serve_client threw

            total_bytes_sent += progress.bytes_sent.load();
            total_clients++;

            double sec  = chrono::duration<double>(t1 - t0).count();
            double mbps = (sec > 0.0)
                          ? (meta.total_size / (1024.0 * 1024.0)) / sec
                          : 0.0;

            { lock_guard<mutex> lk(g_console_mtx);
              cout << "\n[Client " << id << "] "
                   << (ok ? "done" : "FAILED")
                   << "  " << mbps << " MB/s\n"; }
        });
    }
    for (thread& t : workers)
        if (t.joinable()) t.join();

    auto t_session_end = chrono::high_resolution_clock::now();

    IO_COUNTERS io_after{};
    GetProcessIoCounters(GetCurrentProcess(), &io_after);
    uint64_t disk_reads = io_after.ReadOperationCount - io_before.ReadOperationCount;
    uint64_t disk_bytes = io_after.ReadTransferCount  - io_before.ReadTransferCount;

    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));

    double total_mb = total_bytes_sent.load() / (1024.0 * 1024.0);

    cout << "\n=== Session Stats ===\n";
    cout << "Clients       : " << total_clients.load()  << "\n";
    cout << "Chunk size    : " << (CHUNK_SIZE / 1024)   << " KB\n";
    cout << "Total sent    : " << total_mb              << " MB\n";

    if (session_started && total_clients.load() > 0)
    {
        double sec  = chrono::duration<double>(t_session_end - t_session_start).count();
        double agg  = (sec > 0.0) ? total_mb / sec : 0.0;
        cout << "Aggregate     : " << agg << " MB/s\n";
    }

    cout << "Disk reads    : " << disk_reads << " ops (" << disk_bytes << " bytes)\n";
    if (disk_reads == 0)
        cout << "              ^ GOOD: served from page cache\n";
    else
        cout << "              ^ NOTE: some disk reads occurred\n";
    cout << "Peak memory   : " << (pmc.PeakWorkingSetSize / (1024.0 * 1024.0)) << " MB\n";

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
