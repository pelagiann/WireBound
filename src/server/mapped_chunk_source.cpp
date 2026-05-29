#include "mapped_chunk_source.hpp"
#include "picosha2.h"

#include <stdexcept>
#include <string>
#include <iostream>

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
        0,
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
    if (chunk_size == 0) {
        CloseHandle(hFile_); hFile_ = INVALID_HANDLE_VALUE;
        throw std::invalid_argument("MappedChunkSource: chunk_size must be > 0");
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
	picosha2::hash256_one_by_one hasher;
	hasher.init();

	const size_t HASH_BLOCK_SIZE = 16 * 1024 * 1024; // 16 MB

	uint64_t processed = 0;

	while (processed < static_cast<uint64_t>(fileSize.QuadPart))
	{
    	size_t remaining =
        	static_cast<size_t>(fileSize.QuadPart - processed);

    	size_t block =
        	(remaining > HASH_BLOCK_SIZE)
        	? HASH_BLOCK_SIZE
        	: remaining;

    	hasher.process(
       	 	view_ + processed,
        	view_ + processed + block
    	);

    	processed += block;

   		 std::cout << "\rHashing: "
         << processed / (1024 * 1024)
         << " MB";
	}

	hasher.finish();

	std::string sha256_hex =
    picosha2::get_hash_hex_string(hasher);

	std::cout << "\nSHA-256 complete.\n";
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
    if (index >= meta_.chunk_count)
        throw std::out_of_range("MappedChunkSource::get_chunk: index " +
                                std::to_string(index) + " >= chunk_count " +
                                std::to_string(meta_.chunk_count));
    const uint64_t offset = index * meta_.chunk_size;

    const size_t len = (offset + meta_.chunk_size <= meta_.total_size)
                       ? static_cast<size_t>(meta_.chunk_size)
                       : static_cast<size_t>(meta_.total_size - offset);

    return ChunkView{ view_ + offset, len, index };
}

}
