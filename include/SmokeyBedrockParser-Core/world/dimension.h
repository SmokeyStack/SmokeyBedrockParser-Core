#pragma once

#include <climits>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "SmokeyBedrockParser-Core/logger.h"
#include "SmokeyBedrockParser-Core/minecraft/block.h"
#include "SmokeyBedrockParser-Core/world/chunk.h"

namespace smokey_bedrock_parser {
	const std::vector<std::string> dimension_id_names{ "overworld","nether","the-end" };

	class Dimension {
	public:
		Dimension() {
			dimension_name = "(UNKNOWN)";
			dimension_id = -1;
			chunk_bounds_valid = false;
			min_chunk_x = INT32_MAX;
			max_chunk_x = INT32_MIN;
			min_chunk_z = INT32_MAX;
			max_chunk_z = INT32_MIN;
		}

		void set_dimension_name(std::string name) {
			dimension_name = name;
		}

		std::string get_dimension_name() {
			return dimension_name;
		}

		void set_dimension_id(int id) {
			dimension_id = id;
		}

		int get_dimension_id() {
			return dimension_id;
		}

		int get_min_chunk_x() {
			return min_chunk_x;
		}

		int get_max_chunk_x() {
			return max_chunk_x;
		}

		int get_min_chunk_z() {
			return min_chunk_z;
		}

		int get_max_chunk_z() {
			return max_chunk_z;
		}

		void UnsetChunkBoundsValid() {
			min_chunk_x = INT32_MAX;
			max_chunk_x = INT32_MIN;
			min_chunk_z = INT32_MAX;
			max_chunk_z = INT32_MIN;
			chunk_bounds_valid = false;
		}

		bool GetChunkBoundsValid() {
			return chunk_bounds_valid;
		}

		void SetChunkBoundsValid() {
			chunk_bounds_valid = true;
		}

		void ReportChunkBounds() {
			log::info("Bounds (chunk): Dimension Id={} X=({} {}) Z=({} {})",
				dimension_id, min_chunk_x, max_chunk_x, min_chunk_z, max_chunk_z);
		}

		void AddToChunkBounds(int chunk_x, int chunk_z) {
			min_chunk_x = std::min(min_chunk_x, chunk_x);
			max_chunk_x = std::max(max_chunk_x, chunk_x);
			min_chunk_z = std::min(min_chunk_z, chunk_z);
			max_chunk_z = std::max(max_chunk_z, chunk_z);
		}

		int AddChunk(int chunk_format_version, int chunk_x, int chunk_y, int chunk_z, const char* buffer,
			size_t buffer_length) {
			ChunkKey key(chunk_x, chunk_z);

			if (chunk_format_version == 7) {
				if (!chunks_has_key(chunks, key))
					chunks[key] = std::unique_ptr<Chunk>(new Chunk());

				chunks[key]->ParseChunk(chunk_x, chunk_y, chunk_z, buffer, buffer_length, dimension_id, dimension_name);

				return 0;
			}
			else {
				log::error("Unknown chunk format version (version = {})", chunk_format_version);

				return 1;
			}
		};

		bool DoesChunkExist(int x, int z) {
			ChunkKey key(x, z);

			return (chunks_has_key(chunks, key)) ? true : false;
		};

		nlohmann::json GetChunk(int x, int z, int y) {
			if (!DoesChunkExist(x, z)) return NULL;

			nlohmann::json json;
			json["chunk_x"] = x;
			json["chunk_y"] = y;
			json["chunk_z"] = z;
			json["blocks"] = chunks[ChunkKey(x, z)]->blocks;

			return json;
		}

		std::string GetBlock(int x, int y, int z) {
			int chunk_x = floor(x / 16);
			int chunk_y = floor(y / 16);
			int chunk_z = floor(z / 16);

			if (!DoesChunkExist(chunk_x, chunk_z)) return NULL;

			return chunks[ChunkKey(x, z)]->blocks[chunk_y + 4][x % 16][z % 16][y % 16];
		}
	private:
		std::string dimension_name;
		int dimension_id;
		typedef std::pair<int, int> ChunkKey;
		typedef std::map<ChunkKey, std::unique_ptr<Chunk>> ChunkMap;
		ChunkMap chunks;
		int min_chunk_x, max_chunk_x;
		int min_chunk_z, max_chunk_z;
		bool chunk_bounds_valid;

		bool chunks_has_key(const ChunkMap& m, const ChunkKey& k) {
			return m.find(k) != m.end();
		}
	};
} // namespace smokey_bedrock_parser