#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <memory>
#include <variant>
#include <vector>

namespace LeagueModel::Assets
{
	struct BinObject;
	struct BinArray;
	struct BinMap;
	struct BinValue
	{
		using Storage = std::variant<std::monostate, bool, std::int8_t, std::uint8_t, std::int16_t, std::uint16_t, std::int32_t, std::uint32_t, std::int64_t, std::uint64_t, float, std::string, std::shared_ptr<BinObject>, std::shared_ptr<BinArray>, std::shared_ptr<BinMap>>;
		Storage data;
		template<typename T> const T* As() const { return std::get_if<T>(&data); }
		const BinObject* Object() const;
		const BinArray* Array() const;
		const BinMap* Map() const;
	};
	struct BinObject
	{
		std::uint32_t typeHash = 0;
		std::unordered_map<std::uint32_t, BinValue> fields;
		bool complete = true;
		std::string decodeError;
		const BinValue* Find(std::uint32_t hash) const;
	};
	struct BinArray { std::vector<BinValue> values; };
	struct BinMap { std::vector<std::pair<BinValue, BinValue>> values; };
	// The PROP container index. Value decoding is intentionally separate so callers
	// can inspect links/root objects without retaining invalid pointers into input.
	class BinDocument
	{
	public:
		bool Load(std::span<const std::uint8_t> payload, std::string* error = nullptr);
		const std::vector<std::string>& LinkedFiles() const { return m_linkedFiles; }
		bool HasRoot(std::uint32_t hash) const { return m_roots.contains(hash); }
		std::span<const std::uint8_t> RootPayload(std::uint32_t hash) const;
		const BinObject* DecodeRoot(std::uint32_t hash, std::string* error = nullptr) const;
		std::uint32_t Version() const { return m_version; }

	private:
		std::vector<std::uint8_t> m_payload;
		std::vector<std::string> m_linkedFiles;
		std::unordered_map<std::uint32_t, std::pair<size_t, size_t>> m_roots;
		std::uint32_t m_version = 0;
		mutable std::unordered_map<std::uint32_t, std::shared_ptr<BinObject>> m_decodedRoots;
	};

	std::uint32_t BinFieldHash(const std::string& name);
}
