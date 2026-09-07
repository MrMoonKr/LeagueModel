#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace LeagueModel::Assets
{
	class BinaryReader
	{
	public:
		explicit BinaryReader(std::span<const std::uint8_t> data) : m_data(data) {}
		explicit BinaryReader(const std::vector<std::uint8_t>& data) : BinaryReader(std::span(data)) {}

		size_t Tell() const { return m_offset; }
		size_t Remaining() const { return m_data.size() - m_offset; }
		void Seek(size_t offset);
		std::span<const std::uint8_t> Read(size_t size);
		std::uint8_t ReadU8();
		std::uint16_t ReadU16();
		std::int16_t ReadI16();
		std::uint32_t ReadU32();
		std::int32_t ReadI32();
		std::uint64_t ReadU64();
		float ReadF32();
		std::string ReadFixedString(size_t size);
		std::string ReadCString();
		std::string ReadOffsetString();

	private:
		template<typename T> T ReadValue();
		std::span<const std::uint8_t> m_data;
		size_t m_offset = 0;
	};
}
