#include "assets/binary_reader.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace LeagueModel::Assets
{
	void BinaryReader::Seek(size_t offset)
	{
		if (offset > m_data.size()) throw std::out_of_range("BinaryReader seek outside input");
		m_offset = offset;
	}

	std::span<const std::uint8_t> BinaryReader::Read(size_t size)
	{
		if (size > Remaining()) throw std::out_of_range("Unexpected end of binary data");
		auto result = m_data.subspan(m_offset, size);
		m_offset += size;
		return result;
	}

	template<typename T> T BinaryReader::ReadValue()
	{
		T value{};
		const auto bytes = Read(sizeof(T));
		std::memcpy(&value, bytes.data(), sizeof(T));
		return value;
	}

	std::uint8_t BinaryReader::ReadU8() { return ReadValue<std::uint8_t>(); }
	std::uint16_t BinaryReader::ReadU16() { return ReadValue<std::uint16_t>(); }
	std::int16_t BinaryReader::ReadI16() { return ReadValue<std::int16_t>(); }
	std::uint32_t BinaryReader::ReadU32() { return ReadValue<std::uint32_t>(); }
	std::int32_t BinaryReader::ReadI32() { return ReadValue<std::int32_t>(); }
	std::uint64_t BinaryReader::ReadU64() { return ReadValue<std::uint64_t>(); }
	float BinaryReader::ReadF32() { return ReadValue<float>(); }

	std::string BinaryReader::ReadFixedString(size_t size)
	{
		const auto bytes = Read(size);
		const auto terminator = std::find(bytes.begin(), bytes.end(), std::uint8_t{ 0 });
		return { reinterpret_cast<const char*>(bytes.data()), static_cast<size_t>(terminator - bytes.begin()) };
	}

	std::string BinaryReader::ReadCString()
	{
		const size_t start = m_offset;
		while (m_offset < m_data.size() && m_data[m_offset] != 0) ++m_offset;
		if (m_offset == m_data.size()) throw std::out_of_range("Unterminated string in binary data");
		std::string value(reinterpret_cast<const char*>(m_data.data() + start), m_offset - start);
		++m_offset;
		return value;
	}

	std::string BinaryReader::ReadOffsetString()
	{
		const size_t fieldOffset = Tell();
		const std::int32_t relativeOffset = ReadI32();
		if (relativeOffset == 0) return {};
		const std::int64_t target = static_cast<std::int64_t>(fieldOffset) + relativeOffset;
		if (target < 0 || static_cast<size_t>(target) >= m_data.size()) throw std::out_of_range("Offset string outside input");
		const size_t resumeOffset = Tell();
		Seek(static_cast<size_t>(target));
		std::string value = ReadCString();
		Seek(resumeOffset);
		return value;
	}
}
