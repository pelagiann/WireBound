#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <string>

// Day 1 throwaway — proves MapViewOfFile works on this machine.
// Usage: mmap_proof.exe <path-to-any-file>
// Prints the first 64 bytes (hex) and the total file size.

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: mmap_proof.exe <file>\n");
        return 1;
    }

    const char* path = argv[1];

    // Open the file for reading only.
    HANDLE hFile = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        printf("ERROR: Could not open file '%s' (error %lu)\n", path, GetLastError());
        return 1;
    }

    // Get the file size.
    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(hFile, &fileSize)) {
        printf("ERROR: GetFileSizeEx failed (error %lu)\n", GetLastError());
        CloseHandle(hFile);
        return 1;
    }

    printf("File: %s\n", path);
    printf("Size: %lld bytes\n", fileSize.QuadPart);

    if (fileSize.QuadPart == 0) {
        printf("File is empty — nothing to map.\n");
        CloseHandle(hFile);
        return 0;
    }

    // Create a file mapping object.
    HANDLE hMap = CreateFileMappingA(
        hFile,
        nullptr,
        PAGE_READONLY,
        0, 0, 
        nullptr
    );

    if (hMap == nullptr) {
        printf("ERROR: CreateFileMapping failed (error %lu)\n", GetLastError());
        CloseHandle(hFile);
        return 1;
    }

    // Map a view of the file into this process's virtual address space.
    const uint8_t* view = static_cast<const uint8_t*>(
        MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0)
    );

    if (view == nullptr) {
        printf("ERROR: MapViewOfFile failed (error %lu)\n", GetLastError());
        CloseHandle(hMap);
        CloseHandle(hFile);
        return 1;
    }

    // Read from the view.
    size_t print_count = (fileSize.QuadPart < 64) ? (size_t)fileSize.QuadPart : 64;

    printf("First %zu bytes (hex):\n  ", print_count);
    for (size_t i = 0; i < print_count; ++i) {
        printf("%02X ", view[i]);
        if ((i + 1) % 16 == 0) printf("\n  ");
    }
    printf("\n");

    // Demonstrate chunk indexing math
    const uint32_t CHUNK_SIZE = 1024 * 1024; // 1 MB
    uint64_t chunk_count = (fileSize.QuadPart + CHUNK_SIZE - 1) / CHUNK_SIZE;
    printf("\nWith 1 MB chunks: %llu chunks total\n", chunk_count);

    // Show the offset and length of chunk 0
    uint64_t offset = 0 * CHUNK_SIZE;
    size_t   len    = (size_t)(
        (offset + CHUNK_SIZE <= (uint64_t)fileSize.QuadPart)
        ? CHUNK_SIZE
        : (fileSize.QuadPart - offset)
    );
    printf("Chunk 0: offset=%llu  len=%zu  pointer=%p\n",
           offset, len, (void*)(view + offset));

    // Clean up in reverse order.
    // UnmapViewOfFile releases the virtual address range.
    // CloseHandle(hMap) releases the mapping object.
    // CloseHandle(hFile) closes the file.
    UnmapViewOfFile(view);
    CloseHandle(hMap);
    CloseHandle(hFile);

    printf("\nSuccess — mmap is working on this machine.\n");
    return 0;
}
