#include "mapped_chunk_source.hpp"
#include "picosha2.h"

#include <stdexcept>
#include <string>

namespace maxxcast {

static std::string extract_filename(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

MappedChunkSource::MappedChunkSource(const std::string& filepath,
                                     uint32_t chunk_size)
{
    hFile_ = CreateFileA(
        filepath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr
    );
    if (hFile_ == INVALID_HANDLE_VALUE) {
        throw std::runtime_error(
            "MappedChunkSource: cannot open '" + filepath +
            "' (error " + std::to_string(GetLastError()) + ")"
        );
    }

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(hFile_, &fileSize)) {
        CloseHandle(hFile_); hFile_ = INVALID_HANDLE_VALUE;
        throw std::runtime_error(
            "MappedChunkSource: GetFileSizeEx failed (error " +
            std::to_string(GetLastError()) + ")"
        );
    }
    if (fileSize.QuadPart == 0) {
        CloseHandle(hFile_); hFile_ = INVALID_HANDLE_VALUE;
        throw std::runtime_error(
            "MappedChunkSource: file is empty: " + filepath
        );
    }

    hMap_ = CreateFileMappingA(
        hFile_,
        nullptr,
        PAGE_READONLY,
        0, 0,
        nullptr
    );
    if (hMap_ == nullptr) {
        CloseHandle(hFile_); hFile_ = INVALID_HANDLE_VALUE;
        throw std::runtime_error(
            "MappedChunkSource: CreateFileMapping failed (error " +
            std::to_string(GetLastError()) + ")"
        );
    }

    view_ = static_cast<const uint8_t*>(
        MapViewOfFile(hMap_, FILE_MAP_READ, 0, 0, 0)
    );
    if (view_ == nullptr) {
        CloseHandle(hMap_);  hMap_  = nullptr;
        CloseHandle(hFile_); hFile_ = INVALID_HANDLE_VALUE;
        throw std::runtime_error(
            "MappedChunkSource: MapViewOfFile failed (error " +
            std::to_string(GetLastError()) + ")"
        );
    }

    std::string sha256_hex = picosha2::hash256_hex_string(
        view_,
        view_ + fileSize.QuadPart
    );

    meta_.filename    = extract_filename(filepath);
    meta_.total_size  = static_cast<uint64_t>(fileSize.QuadPart);
    meta_.chunk_size  = chunk_size;
    meta_.chunk_count = (meta_.total_size + chunk_size - 1) / chunk_size;
    meta_.sha256_hex  = std::move(sha256_hex);
}

MappedChunkSource::~MappedChunkSource() {
    if (view_  != nullptr)              UnmapViewOfFile(view_);
    if (hMap_  != nullptr)              CloseHandle(hMap_);
    if (hFile_ != INVALID_HANDLE_VALUE) CloseHandle(hFile_);
}

const FileMeta& MappedChunkSource::metadata() const {
    return meta_;
}

ChunkView MappedChunkSource::get_chunk(uint64_t index) const {
    const uint64_t offset = index * meta_.chunk_size;

    const size_t len = (offset + meta_.chunk_size <= meta_.total_size)
                       ? static_cast<size_t>(meta_.chunk_size)
                       : static_cast<size_t>(meta_.total_size - offset);

    return ChunkView{ view_ + offset, len, index };
}

}
