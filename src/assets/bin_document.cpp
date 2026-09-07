#include "assets/bin_document.hpp"
#include "assets/binary_reader.hpp"

#include <cctype>
#include <cstring>
#include <stdexcept>

namespace LeagueModel::Assets
{
	namespace
	{
		enum class BinType : std::uint8_t { Empty, Bool, S8, U8, S16, U16, S32, U32, S64, U64, Float, Vec2, Vec3, Vec4, Mat4, Rgba, String, Hash, Path, Container = 0x80, Container2, Struct, Embedded, Link, Array, Map, Flag };
		BinValue DecodeValue(BinaryReader& reader, BinType type);
		BinValue DecodeObject(BinaryReader& reader)
		{
			const std::uint32_t typeHash = reader.ReadU32();
			if (typeHash == 0) return { std::make_shared<BinObject>() };
			const std::uint32_t length = reader.ReadU32();
			const size_t start = reader.Tell();
			const std::uint16_t count = reader.ReadU16();
			auto object = std::make_shared<BinObject>(); object->typeHash = typeHash;
			for (std::uint16_t index = 0; index < count; ++index) { const auto key = reader.ReadU32(); object->fields.emplace(key, DecodeValue(reader, static_cast<BinType>(reader.ReadU8()))); }
			if (reader.Tell() != start + length) throw std::runtime_error("BIN object length mismatch");
			return { object };
		}
		BinValue DecodeContainer(BinaryReader& reader)
		{
			const auto elementType = static_cast<BinType>(reader.ReadU8()); const auto length = reader.ReadU32(); const size_t start = reader.Tell(); const auto count = reader.ReadU32();
			auto array = std::make_shared<BinArray>(); array->values.reserve(count);
			for (std::uint32_t index = 0; index < count; ++index) array->values.push_back(DecodeValue(reader, elementType));
			if (reader.Tell() != start + length) throw std::runtime_error("BIN container length mismatch"); return { array };
		}
		BinValue DecodeValue(BinaryReader& reader, BinType type)
		{
			// WadChunkLink (0x12) was inserted after legacy BINs shipped. Their
			// complex type IDs are stored in the compact 0x26..0x2D range.
			const std::uint8_t rawType = static_cast<std::uint8_t>(type);
			if (rawType >= 0x26 && rawType <= 0x2d)
				type = static_cast<BinType>(rawType + 0x5a);
			switch (type)
			{
			case BinType::Empty: return {};
			case BinType::Bool: case BinType::Flag: return { bool(reader.ReadU8()) };
			case BinType::S8: return { static_cast<std::int8_t>(reader.ReadU8()) }; case BinType::U8: return { reader.ReadU8() };
			case BinType::S16: return { reader.ReadI16() }; case BinType::U16: return { reader.ReadU16() };
			case BinType::S32: return { reader.ReadI32() }; case BinType::U32: case BinType::Hash: case BinType::Link: return { reader.ReadU32() };
			case BinType::S64: return { static_cast<std::int64_t>(reader.ReadU64()) }; case BinType::U64: case BinType::Path: return { reader.ReadU64() };
			case BinType::Float: return { reader.ReadF32() };
			case BinType::String: { const auto length = reader.ReadU16(); const auto text = reader.Read(length); return { std::string(reinterpret_cast<const char*>(text.data()), text.size()) }; }
			case BinType::Struct: case BinType::Embedded: return DecodeObject(reader);
			case BinType::Container: case BinType::Container2: return DecodeContainer(reader);
			case BinType::Array: { const auto childType = static_cast<BinType>(reader.ReadU8()); const auto count = reader.ReadU8(); auto array = std::make_shared<BinArray>(); array->values.reserve(count); for (std::uint8_t index = 0; index < count; ++index) array->values.push_back(DecodeValue(reader, childType)); return { array }; }
			case BinType::Map: { const auto keyType = static_cast<BinType>(reader.ReadU8()); const auto valueType = static_cast<BinType>(reader.ReadU8()); const auto length = reader.ReadU32(); const size_t start = reader.Tell(); const auto count = reader.ReadU32(); auto map = std::make_shared<BinMap>(); for (std::uint32_t index = 0; index < count; ++index) map->values.emplace_back(DecodeValue(reader, keyType), DecodeValue(reader, valueType)); if (reader.Tell() != start + length) throw std::runtime_error("BIN map length mismatch"); return { map }; }
			case BinType::Vec2: reader.Read(8); return {}; case BinType::Vec3: reader.Read(12); return {}; case BinType::Vec4: reader.Read(16); return {}; case BinType::Mat4: reader.Read(64); return {}; case BinType::Rgba: reader.Read(4); return {};
			default: throw std::runtime_error("Unsupported BIN value type " + std::to_string(static_cast<unsigned>(type)) + " at offset " + std::to_string(reader.Tell()));
			}
		}
	}

	const BinObject* BinValue::Object() const { const auto value = As<std::shared_ptr<BinObject>>(); return value && *value ? value->get() : nullptr; }
	const BinArray* BinValue::Array() const { const auto value = As<std::shared_ptr<BinArray>>(); return value && *value ? value->get() : nullptr; }
	const BinMap* BinValue::Map() const { const auto value = As<std::shared_ptr<BinMap>>(); return value && *value ? value->get() : nullptr; }
	const BinValue* BinObject::Find(std::uint32_t hash) const { const auto value = fields.find(hash); return value == fields.end() ? nullptr : &value->second; }
	std::uint32_t BinFieldHash(const std::string& name)
	{
		std::uint32_t hash = 0x811c9dc5;
		for (const unsigned char character : name)
			hash = (hash ^ static_cast<std::uint8_t>(std::tolower(character))) * 0x01000193;
		return hash;
	}

	bool BinDocument::Load(std::span<const std::uint8_t> payload, std::string* error)
	{
		m_payload.assign(payload.begin(), payload.end());
		m_linkedFiles.clear();
		m_roots.clear();
		m_version = 0;
		m_decodedRoots.clear();
		try
		{
			BinaryReader reader(m_payload);
			const auto magic = reader.Read(4);
			if (std::memcmp(magic.data(), "PROP", 4) != 0) throw std::runtime_error("BIN has no PROP header");
			m_version = reader.ReadU32();
			if (m_version > 3) throw std::runtime_error("Unsupported BIN version " + std::to_string(m_version));
			if (m_version >= 2)
			{
				const std::uint32_t linkCount = reader.ReadU32();
				for (std::uint32_t index = 0; index < linkCount; ++index)
				{
					const std::uint16_t length = reader.ReadU16();
					const auto text = reader.Read(length);
					m_linkedFiles.emplace_back(reinterpret_cast<const char*>(text.data()), text.size());
				}
			}
			const std::uint32_t entryCount = reader.ReadU32();
			if (entryCount > reader.Remaining() / 4) throw std::runtime_error("Invalid BIN type table size");
			reader.Read(static_cast<size_t>(entryCount) * 4); // Root type hashes, not needed for lookup.
			for (std::uint32_t index = 0; index < entryCount; ++index)
			{
				const std::uint32_t length = reader.ReadU32();
				const size_t rootStart = reader.Tell();
				if (length < 6 || length > reader.Remaining()) throw std::runtime_error("Invalid BIN root length");
				const std::uint32_t rootHash = reader.ReadU32();
				reader.ReadU16(); // field count; decoded by the next BinValue milestone.
				m_roots[rootHash] = { rootStart, length };
				reader.Seek(rootStart + length);
			}
			return true;
		}
		catch (const std::exception& exception)
		{
			m_payload.clear();
			m_linkedFiles.clear();
			m_roots.clear();
			if (error) *error = exception.what();
			return false;
		}
	}

	std::span<const std::uint8_t> BinDocument::RootPayload(std::uint32_t hash) const
	{
		const auto found = m_roots.find(hash);
		if (found == m_roots.end()) return {};
		return std::span(m_payload).subspan(found->second.first, found->second.second);
	}

	const BinObject* BinDocument::DecodeRoot(std::uint32_t hash, std::string* error) const
	{
		if (const auto cached = m_decodedRoots.find(hash); cached != m_decodedRoots.end()) return cached->second.get();
		try
		{
			BinaryReader reader(RootPayload(hash));
			reader.ReadU32();
			const auto count = reader.ReadU16();
			auto object = std::make_shared<BinObject>();
			for (std::uint16_t index = 0; index < count; ++index)
			{
				try { const auto key = reader.ReadU32(); object->fields.emplace(key, DecodeValue(reader, static_cast<BinType>(reader.ReadU8()))); }
				catch (const std::exception& exception)
				{
					// An unknown value has no reliable payload length. The root's declared
					// boundary is safe, so preserve decoded fields and stop only this root.
					object->complete = false;
					object->decodeError = exception.what();
					break;
				}
			}
			if (object->complete && reader.Remaining() != 0) throw std::runtime_error("BIN root length mismatch");
			m_decodedRoots.emplace(hash, object);
			return object.get();
		}
		catch (const std::exception& exception) { if (error) *error = exception.what(); return nullptr; }
	}
}
