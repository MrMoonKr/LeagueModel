#include "assets/wad_archive.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cctype>
#include <cstring>

extern "C"
{
#include <zstd.h>
}

namespace LeagueModel::Assets
{
#pragma pack(push, 1)
	struct WadHeaderV3
	{
		char magic[2];
		std::uint8_t major;
		std::uint8_t minor;
		std::array<std::uint8_t, 256> ecdsa;
		std::uint64_t checksum;
		std::uint32_t fileCount;
	};
	struct WadFileData
	{
		std::uint64_t pathHash;
		std::uint32_t offset;
		std::uint32_t compressedSize;
		std::uint32_t fileSize;
		std::uint8_t typeData;
		std::uint8_t duplicate;
		std::uint16_t firstSubchunkIndex;
		std::uint64_t sha256;
	};
#pragma pack(pop)
	static_assert(sizeof(WadHeaderV3) == 272);
	static_assert(sizeof(WadFileData) == 32);

	static std::string HashPath(std::uint64_t hash)
	{
		std::ostringstream stream;
		stream << "@wad/" << std::hex << std::setfill('0') << std::setw(16) << hash;
		return stream.str();
	}

	static std::string SubchunkTocPath(const std::filesystem::path& wadPath)
	{
		std::string path = wadPath.generic_string();
		std::transform(path.begin(), path.end(), path.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		const size_t dataOffset = path.find("data/final");
		if (dataOffset == std::string::npos) return {};
		path.erase(0, dataOffset);
		const size_t extension = path.rfind(".client");
		if (extension == std::string::npos) return {};
		path.replace(extension, std::string::npos, ".subchunktoc");
		return path;
	}

	WadArchive::WadArchive(std::filesystem::path filePath, const GameHashIndex* hashIndex, std::string archiveId) :
		m_filePath(std::move(filePath)),
		m_archiveId(archiveId.empty() ? "wad:" + m_filePath.filename().string() : std::move(archiveId)),
		m_hashIndex(hashIndex)
	{
	}

	std::vector<AssetEntry> WadArchive::Scan()
	{
		std::ifstream file(m_filePath, std::ios::binary);
		if (!file) throw AssetArchiveError("Cannot open WAD: " + m_filePath.string());
		WadHeaderV3 header{};
		if (!file.read(reinterpret_cast<char*>(&header), sizeof(header))) throw AssetArchiveError("WAD header is truncated: " + m_filePath.string());
		if (header.magic[0] != 'R' || header.magic[1] != 'W' || header.major != 3)
			throw AssetArchiveError("Unsupported WAD version: " + m_filePath.string());

		m_records.clear();
		m_entries.clear();
		const std::uint64_t fileLength = std::filesystem::file_size(m_filePath);
		for (std::uint32_t index = 0; index < header.fileCount; ++index)
		{
			WadFileData source{};
			if (!file.read(reinterpret_cast<char*>(&source), sizeof(source))) throw AssetArchiveError("WAD entry table is truncated: " + m_filePath.string());
			if (source.offset > fileLength || source.compressedSize > fileLength - source.offset)
				throw AssetArchiveError("WAD entry range is outside archive: " + m_filePath.string());
			FileRecord record{ source.pathHash, source.offset, source.compressedSize, source.fileSize, source.typeData, source.firstSubchunkIndex };
			m_records[record.hash] = record;
			const std::string path = m_hashIndex && m_hashIndex->Lookup(record.hash) ? *m_hashIndex->Lookup(record.hash) : HashPath(record.hash);
			m_entries.emplace(path, AssetEntry{ { m_archiveId, path }, record.offset, record.packedSize, record.fileSize, record.hash });
		}
		return Entries();
	}

	std::vector<AssetEntry> WadArchive::Entries() const
	{
		std::vector<AssetEntry> result;
		result.reserve(m_entries.size());
		for (const auto& [_, entry] : m_entries) result.push_back(entry);
		std::sort(result.begin(), result.end(), [](const AssetEntry& left, const AssetEntry& right) { return left.path.path < right.path.path; });
		return result;
	}

	std::vector<std::uint8_t> WadArchive::ReadFileRange(std::uint64_t offset, std::uint64_t size) const
	{
		std::ifstream file(m_filePath, std::ios::binary);
		if (!file) throw AssetArchiveError("Cannot open WAD: " + m_filePath.string());
		file.seekg(static_cast<std::streamoff>(offset));
		std::vector<std::uint8_t> data(static_cast<size_t>(size));
		if (!data.empty() && !file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size())))
			throw AssetArchiveError("Cannot read WAD entry: " + m_filePath.string());
		return data;
	}

	std::vector<std::uint8_t> WadArchive::ReadPayload(const AssetEntry& entry) const
	{
		if (!entry.path.archiveName || *entry.path.archiveName != m_archiveId || !entry.hash)
			throw AssetArchiveError("Entry does not belong to " + m_archiveId + ": " + entry.path.ToString());
		const auto found = m_records.find(*entry.hash);
		if (found == m_records.end()) throw AssetArchiveError("Unknown WAD entry: " + entry.path.ToString());
		const FileRecord& record = found->second;
		const std::uint8_t storageType = record.typeData & 0x0f;
		const std::vector<std::uint8_t> packed = ReadFileRange(record.offset, record.packedSize);
		if (storageType == 0)
		{
			if (packed.size() != record.fileSize) throw AssetArchiveError("Uncompressed WAD entry has a mismatched size: " + entry.path.ToString());
			return packed;
		}
		if (storageType == 3)
		{
			std::vector<std::uint8_t> output(record.fileSize);
			const size_t result = ZSTD_decompress(output.data(), output.size(), packed.data(), packed.size());
			if (ZSTD_isError(result) || result != output.size())
				throw AssetArchiveError("Zstd decompression failed for: " + entry.path.ToString());
			return output;
		}
		if (storageType == 4)
		{
			if (!m_hashIndex) throw AssetArchiveError("WAD subchunks require a GameHashIndex: " + entry.path.ToString());
			const std::string tocPath = SubchunkTocPath(m_filePath);
			const std::uint64_t* tocHash = m_hashIndex->LookupHash(tocPath);
			if (!tocHash) throw AssetArchiveError("Subchunk TOC is not in the hash index: " + tocPath);
			const auto tocRecord = m_records.find(*tocHash);
			if (tocRecord == m_records.end()) throw AssetArchiveError("Subchunk TOC is not in this WAD: " + tocPath);
			AssetEntry tocEntry{ { m_archiveId, tocPath }, tocRecord->second.offset, tocRecord->second.packedSize, tocRecord->second.fileSize, *tocHash };
			const std::vector<std::uint8_t> toc = ReadPayload(tocEntry);
			const std::uint32_t frameCount = record.typeData >> 4;
			if (toc.size() % 16 != 0 || static_cast<size_t>(record.firstSubchunkIndex) + frameCount > toc.size() / 16)
				throw AssetArchiveError("Invalid WAD subchunk range: " + entry.path.ToString());
			std::vector<std::uint8_t> output;
			output.reserve(record.fileSize);
			size_t packedOffset = 0;
			for (std::uint32_t frame = 0; frame < frameCount; ++frame)
			{
				const size_t tocOffset = (static_cast<size_t>(record.firstSubchunkIndex) + frame) * 16;
				std::uint32_t compressedSize = 0, uncompressedSize = 0;
				std::memcpy(&compressedSize, toc.data() + tocOffset, sizeof(compressedSize));
				std::memcpy(&uncompressedSize, toc.data() + tocOffset + 4, sizeof(uncompressedSize));
				if (compressedSize > packed.size() - packedOffset) throw AssetArchiveError("Truncated WAD subchunk: " + entry.path.ToString());
				const std::uint8_t* source = packed.data() + packedOffset;
				const size_t previousSize = output.size();
				output.resize(previousSize + uncompressedSize);
				if (compressedSize == uncompressedSize)
					std::memcpy(output.data() + previousSize, source, uncompressedSize);
				else
				{
					const size_t result = ZSTD_decompress(output.data() + previousSize, uncompressedSize, source, compressedSize);
					if (ZSTD_isError(result) || result != uncompressedSize) throw AssetArchiveError("Zstd subchunk decompression failed: " + entry.path.ToString());
				}
				packedOffset += compressedSize;
			}
			if (output.size() != record.fileSize) throw AssetArchiveError("WAD subchunk output has a mismatched size: " + entry.path.ToString());
			return output;
		}
		throw AssetArchiveError("Unsupported WAD storage type " + std::to_string(storageType) + ": " + entry.path.ToString());
	}
}
