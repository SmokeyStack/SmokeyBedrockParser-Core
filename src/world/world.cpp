#include "SmokeyBedrockParser-Core/world/world.h"

#include <leveldb/cache.h>
#include <leveldb/env.h>
#include <leveldb/filter_policy.h>
#include <regex>

#include "SmokeyBedrockParser-Core/json.hpp"
#include "SmokeyBedrockParser-Core/logger.h"
#include "SmokeyBedrockParser-Core/nbt.h"

static int8_t ParseInt8(const char* p, int32_t startByte) {
	return (p[startByte] & 0xff);
}

static int32_t ParseInt32(const char* p, int32_t startByte) {
	int32_t ret;

	memcpy(&ret, &p[startByte], 4);

	return ret;
}

static std::pair<bool, int32_t> IsChunkKey(std::string_view key) {
	auto tag_test = [](char tag) {
		return ((33 <= tag && tag <= 64) || tag == 118);
		};

	if (key.size() == 9 || key.size() == 10) return std::make_pair(tag_test(key[8]), ParseInt8(key.data(), 8));
	else if (key.size() == 13 || key.size() == 14) return std::make_pair(tag_test(key[12]), ParseInt8(key.data(), 12));

	return std::make_pair(false, 0);
}

// https://learn.microsoft.com/en-us/minecraft/creator/documents/actorstorage
enum class ChunkTag : char {
	Data3D = 43,
	Version, // This was moved to the front as needed for the extended heights feature. Old chunks will not have this data.
	Data2D,
	Data2DLegacy,
	SubChunkPrefix,
	LegacyTerrain,
	BlockEntity,
	Entity,
	PendingTicks,
	LegacyBlockExtraData,
	BiomeState,
	FinalizedState,
	ConversionData, // data that the converter provides, that are used at runtime for things like blending
	BorderBlocks,
	HardcodedSpawners,
	RandomTicks,
	CheckSums,
	GenerationSeed,
	GeneratedPreCavesAndCliffsBlending = 61, // not used, DON'T REMOVE
	BlendingBiomeHeight = 62, // not used, DON'T REMOVE
	MetaDataHash,
	BlendingData,
	ActorDigestVersion,
	LegacyVersion = 118,
};

struct ChunkData {
	int chunk_x;
	int chunk_z;
	int chunk_dimension_id;
	ChunkTag chunk_tag;
	int chunk_type_sub;
	std::string dimension_name;
};

template<>
struct fmt::formatter<ChunkTag> : fmt::formatter<std::string>
{
	auto format(ChunkTag my, format_context& ctx) const -> decltype(ctx.out())
	{
		switch (my) {
		case ChunkTag::Data3D:
			return format_to(ctx.out(), "Data3D");
		case ChunkTag::Version:
			return format_to(ctx.out(), "Version");
		case ChunkTag::Data2D:
			return format_to(ctx.out(), "Data2D");
		case ChunkTag::Data2DLegacy:
			return format_to(ctx.out(), "Data2DLegacy");
		case ChunkTag::SubChunkPrefix:
			return format_to(ctx.out(), "SubChunkPrefix");
		case ChunkTag::LegacyTerrain:
			return format_to(ctx.out(), "LegacyTerrain");
		case ChunkTag::BlockEntity:
			return format_to(ctx.out(), "BlockEntity");
		case ChunkTag::Entity:
			return format_to(ctx.out(), "Entity");
		case ChunkTag::PendingTicks:
			return format_to(ctx.out(), "PendingTicks");
		case ChunkTag::LegacyBlockExtraData:
			return format_to(ctx.out(), "LegacyBlockExtraData");
		case ChunkTag::BiomeState:
			return format_to(ctx.out(), "BiomeState");
		case ChunkTag::FinalizedState:
			return format_to(ctx.out(), "FinalizedState");
		case ChunkTag::ConversionData:
			return format_to(ctx.out(), "ConversionData");
		case ChunkTag::BorderBlocks:
			return format_to(ctx.out(), "BorderBlocks");
		case ChunkTag::HardcodedSpawners:
			return format_to(ctx.out(), "HardcodedSpawners");
		case ChunkTag::RandomTicks:
			return format_to(ctx.out(), "RandomTicks");
		case ChunkTag::CheckSums:
			return format_to(ctx.out(), "CheckSums");
		case ChunkTag::GenerationSeed:
			return format_to(ctx.out(), "GenerationSeed");
		case ChunkTag::GeneratedPreCavesAndCliffsBlending:
			return format_to(ctx.out(), "GeneratedPreCavesAndCliffsBlending");
		case ChunkTag::BlendingBiomeHeight:
			return format_to(ctx.out(), "BlendingBiomeHeight");
		case ChunkTag::MetaDataHash:
			return format_to(ctx.out(), "MetaDataHash");
		case ChunkTag::BlendingData:
			return format_to(ctx.out(), "BlendingData");
		case ChunkTag::ActorDigestVersion:
			return format_to(ctx.out(), "ActorDigestVersion");
		case ChunkTag::LegacyVersion:
			return format_to(ctx.out(), "LegacyVersion");
		default:
			return format_to(ctx.out(), "Unknown");
		}
	}
};

static std::vector<char> HexToBytes(const std::string& hex) {
	std::vector<char> bytes;

	for (unsigned int i = 0; i < hex.length(); i += 2) {
		std::string byteString = hex.substr(i, 2);
		char byte = (char)strtol(byteString.c_str(), NULL, 16);
		bytes.push_back(byte);
	}

	return bytes;
}

static std::string SliceToHexString(leveldb::Slice slice) {
	std::stringstream ss;
	ss << std::hex << std::setfill('0');

	for (size_t a = 0; a < slice.size(); a++)
		ss << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(slice[a]));

	return ss.str();
}

static std::vector<char> CoordsToDbKey(int x, int z, int y) {
	std::string temp_data;
	uint32_t chunk_x = floor(x / 16.0f);
	int chunk_z = floor(z / 16.0f);
	uint32_t chunk_y = floor(y / 16.0f);

	std::stringstream stream;
	chunk_x = _byteswap_ulong(chunk_x);
	chunk_z = _byteswap_ulong(chunk_z);
	stream << std::setfill('0') << std::setw(sizeof(int) * 2) << std::hex << chunk_x;
	temp_data += stream.str();
	stream.str(std::string());
	stream << std::setfill('0') << std::setw(sizeof(int) * 2) << std::hex << chunk_z;
	temp_data += stream.str();
	stream.str(std::string());
	temp_data += "2f";
	stream << std::setfill('0') << std::setw(sizeof(int) * 2) << std::hex << chunk_y;
	temp_data += stream.str().substr(stream.str().size() - 2);
	stream.str(std::string());

	return HexToBytes(temp_data);
}

namespace {
	class NullLogger : public leveldb::Logger {
	public:
		void Logv(const char*, va_list) override {}
	};
}

namespace smokey_bedrock_parser {
	MinecraftWorldLevelDB::MinecraftWorldLevelDB() {
		db = nullptr;

		leveldb_read_options.fill_cache = false;
		// suggestion from leveldb/mcpe_sample_setup.cpp
		leveldb_read_options.decompress_allocator = new leveldb::DecompressAllocator();


		db_options = std::make_unique<leveldb::Options>();
		//dbOptions->compressors[0] = new leveldb::ZlibCompressor();
		db_options->create_if_missing = false;

		// this filter is supposed to reduce disk reads - light testing indicates that it is faster when doing 'html-all'

		db_options->filter_policy = leveldb::NewBloomFilterPolicy(10);


		db_options->block_size = 4096;

		//create a 40 mb cache (we use this on ~1gb devices)
		db_options->block_cache = leveldb::NewLRUCache(40 * 1024 * 1024);

		//create a 4mb write buffer, to improve compression and touch the disk less
		db_options->write_buffer_size = 4 * 1024 * 1024;
		db_options->info_log = new NullLogger();
		db_options->compression = leveldb::kZlibRawCompression;

		for (int32_t i = 0; i < 3; i++) {
			dimensions.push_back(std::make_unique<Dimension>());
			dimensions[i]->set_dimension_id(i);
			dimensions[i]->UnsetChunkBoundsValid();
			dimensions[i]->set_dimension_name(dimension_id_names[i]);
		}
	}

	int MinecraftWorldLevelDB::init(std::string db_directory) {
		int result = ParseLevelFile(std::string(db_directory + "/level.dat"));

		if (result != 0) {
			log::error("Failed to parse level.dat file.  Exiting...");

			return -1;
		}

		result = ParseLevelName(std::string(db_directory + "/levelname.txt"));

		if (result != 0)
			log::warn("WARNING: Failed to parse levelname.txt file.");

		return 0;
	}

	int MinecraftWorldLevelDB::OpenDB(std::string db_directory) {
		log::info("Open DB: directory={}", db_directory);
		db_options = std::make_unique<leveldb::Options>();
		leveldb::Status status = leveldb::DB::Open(*db_options, std::string(db_directory + "/db").c_str(), &db);
		log::info("DB Status: {}", status.ToString());

		if (!status.ok())
			log::error("LevelDB operation returned status={}", status.ToString());

		return 0;
	}

	int MinecraftWorldLevelDB::CalculateTotalRecords() {
		bool pass_flag = true;

		for (int32_t i = 0; i < 3; i++)
			if (!dimensions[i]->GetChunkBoundsValid())
				pass_flag = false;
		if (pass_flag)
			return 0;

		for (int32_t i = 0; i < 3; i++)
			dimensions[i]->UnsetChunkBoundsValid();

		log::info("Calculating total leveldb records, this might take a while...");
		int record_count = 0;
		leveldb::Iterator* it = db->NewIterator(leveldb_read_options);
		leveldb::Slice key, value;
		size_t key_size;
		size_t value_size;
		const char* key_name;
		const char* key_data;

		for (it->SeekToFirst(); it->Valid(); it->Next()) {
			record_count++;
			key = it->key();
			key_size = (int)key.size();
			key_name = key.data();
			value = it->value();
			value_size = value.size();
			key_data = value.data();

			if (IsChunkKey({ key_name,key_size }).first) {
				std::string_view chunk_data(key_name, key_size);
				int chunk_dimension_id;
				ChunkTag chunk_tag;

				switch (chunk_data.size()) {
				case 9:
					chunk_dimension_id = 0;
					chunk_tag = (ChunkTag)chunk_data[8];

					break;
				case 10:
					chunk_dimension_id = 0;
					chunk_tag = (ChunkTag)chunk_data[8];

					break;
				case 13:
					chunk_dimension_id = chunk_data[8];
					chunk_tag = (ChunkTag)chunk_data[12];

					if (chunk_dimension_id == 0x32373639)
						chunk_dimension_id = 2;

					if (chunk_dimension_id == 0x33373639)
						chunk_dimension_id = 1;

					// check for new dim id's
					if (chunk_dimension_id != 1 && chunk_dimension_id != 2)
						log::warn("UNKNOWN -- Found new chunk dimension id=0x{:x} -- Did Bedrock finally get custom dimensions? Or did Mojang add a new dimension?", chunk_dimension_id);

					break;
				case 14:
					chunk_dimension_id = chunk_data[8];
					chunk_tag = (ChunkTag)chunk_data[12];

					if (chunk_dimension_id == 0x32373639)
						chunk_dimension_id = 2;

					if (chunk_dimension_id == 0x33373639)
						chunk_dimension_id = 1;

					// check for new dim id's
					if (chunk_dimension_id != 1 && chunk_dimension_id != 2)
						log::warn("UNKNOWN -- Found new chunk dimension id=0x{:x} -- Did Bedrock finally get custom dimensions? Or did Mojang add a new dimension?", chunk_dimension_id);

					break;
				default:
					break;
				}

				if (chunk_tag == ChunkTag::SubChunkPrefix)
					dimensions[chunk_dimension_id]->AddToChunkBounds(chunk_data[0], chunk_data[4]);
			}
		}

		if (!it->status().ok())
			log::warn("LevelDB operation returned status={}", it->status().ToString());

		delete it;

		for (int32_t i = 0; i < 3; i++) {
			dimensions[i]->SetChunkBoundsValid();
			dimensions[i]->ReportChunkBounds();
		}

		log::info("Total records: {}", record_count);
		total_record_count = record_count;

		return 0;
	}

	int MinecraftWorldLevelDB::ParseLevelFile(std::string& file_name) {
		std::ifstream file;
		file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

		try {
			file.open(file_name, std::ios::binary);
		}
		catch (const std::exception& e) {
			log::error("Failed to read levelfile - Error: {}", e.what());

			return -1;
		}

		int32_t format_version, buffer_length;

		try {
			file.read((char*)&format_version, sizeof(int32_t));
			file.read((char*)&buffer_length, sizeof(int32_t));
		}
		catch (const std::exception& e) {
			log::error("Failed to read levelfile - Error: {}", e.what());
		}

		log::info("ParseLevelFile: name={} version={} length={}", file_name, format_version, buffer_length);
		std::pair<int32_t, nlohmann::json> result;

		if (buffer_length > 0) {
			char* buffer = new char[buffer_length];

			file.read(buffer, buffer_length);
			file.close();

			NbtTagList tag_list;

			result = ParseNbt(buffer, buffer_length, tag_list);

			if (result.first == 0) {
				nbt::tag_compound tag_compound = tag_list[0].second->as<nbt::tag_compound>();
				set_world_spawn_x(tag_compound["SpawnX"].as<nbt::tag_int>().get());
				set_world_spawn_y(tag_compound["SpawnY"].as<nbt::tag_int>().get());
				set_world_spawn_z(tag_compound["SpawnZ"].as<nbt::tag_int>().get());

				log::info("Found World Spawn: x={} y={} z={}",
					get_world_spawn_x(),
					get_world_spawn_y(),
					get_world_spawn_z());

				set_world_seed(tag_compound["RandomSeed"].as<nbt::tag_long>().get());
			}

			delete[] buffer;
		}
		else file.close();

		return result.first;
	}

	int MinecraftWorldLevelDB::ParseDB() {
		CalculateTotalRecords();

		log::info("Parsing all leveldb records");

		NbtTagList tag_list;
		int record_count = 0, result = 0;
		leveldb::Slice key, value;
		size_t key_size;
		size_t value_size;
		const char* key_name;
		const char* key_data;
		std::vector<std::string> villages;
		std::vector<uint64_t> actor_ids;
		leveldb::Iterator* it = db->NewIterator(leveldb_read_options);

		// Village Regex
		const std::regex village_dweller_regex("VILLAGE_[0-9a-f\\-]+_DWELLERS");
		const std::regex village_info_regex("VILLAGE_[A-Za-z]+_[0-9a-f\\-]+_INFO");
		const std::regex village_player_regex("VILLAGE_[0-9a-f\\-]+_PLAYERS");
		const std::regex village_poi_regex("VILLAGE_[0-9a-f\\-]+_POI");
		const std::regex map_regex("map_\\-[0-9]+");

		for (it->SeekToFirst(); it->Valid(); it->Next()) {
			key = it->key();
			key_size = (int)key.size();
			key_name = key.data();
			value = it->value();
			value_size = value.size();
			key_data = value.data();
			record_count++;

			if ((record_count % 10000) == 0) {
				double percentage = (double)record_count / (double)total_record_count;
				log::info("Processing records: {} / {} ({:.1f}%)", record_count, total_record_count, percentage * 100.0);
			}

			/**
				Sources for keys: https://minecraft.wiki/w/Bedrock_Edition_level_format
				Sources for more NBT data: https://minecraft.wiki/w/Bedrock_Edition_level_format/Other_data_format
			*/;
			if (IsChunkKey({ key_name,key_size }).first) {
				ParseChunkKey({ key_name, key_size }, key_data, value_size);
			}
			else if (strncmp(key_name, "BiomeData", key_size) == 0) {
				log::info("Found key - BiomeData");

				//ParseNbt(key_data, int32_t(value_size), tag_list);
			}
			else if (strncmp(key_name, "Overworld", key_size) == 0) {
				log::info("Found key - Overworld");

				//ParseNbt(key_data, int32_t(value_size), tag_list);
			}
			else if (strncmp(key_name, "~local_player", key_size) == 0) {
				log::info("Found key - ~local_player");

				//ParseNbt(key_data, int32_t(value_size), tag_list);
			}
			else if ((key_size >= 7) && (strncmp(key_name, "player_", 7) == 0)) {
				std::string player_remote_Id = std::string(&key_name[strlen("player_")], key_size - strlen("player_"));
				log::info("Found key - player_{}", player_remote_Id);

				//ParseNbt(key_data, int32_t(key_data), tag_list);
			}
			else if (strncmp(key_name, "game_flatworldlayers", key_size) == 0) {
				log::info("Found key - game_flatworldlayers");

				//ParseNbt(key_data, int32_t(value_size), tag_list);
			}
			else if (strncmp(key_name, "VILLAGE_", 8) == 0) {
				log::info("Found key - Village-{}", key.ToString());
				char village_id[37];
				char dimension_name[9];

				memcpy(dimension_name, key_name + 8, 9);

				memcpy(village_id, key_name + sizeof(dimension_name) + 9, 36);

				log::info("Village ID-{}", village_id);

				if (std::regex_match(key.ToString(), village_info_regex))
					villages.push_back(std::string((std::string)dimension_name + "_" + (std::string)village_id));
			}
			else if (strncmp(key_name, "AutonomousEntities", key_size) == 0) {
				log::info("Found key - AutonomousEntities");

				//ParseNbt(key_data, int32_t(value_size), tag_list);
			}
			else if (strncmp(key_name, "digp", 4) == 0) {
				for (uint32_t i = 0; i < value_size; i += 8)
					actor_ids.push_back(*(uint64_t*)(key_data + i));
			}
			else if (strncmp(key_name, "actorprefix", 11) == 0) {
				log::info("Found key - actorprefix");

				//ParseNbt(key_data, int32_t(value_size), tag_list);
			}
			//else log::error("Unknown record - key_size={} value_size={}", key_size, value_size);
		}

		log::info("Read {} records", record_count);
		log::info("Status: {}", it->status().ToString());

		if (!it->status().ok())
			log::warn("LevelDB operation returned status={}", it->status().ToString());

		delete it;

		/*

		for (auto actor_id : actor_ids) {
			std::string data;
			char key[19] = "actorprefix";

			memcpy(key + 11, &actor_id, 8);

			NbtTagList actor_list;
			leveldb::ReadOptions read_options;

			db->Get(read_options, leveldb::Slice(key, 19), &data);

			//log::info("{}", ParseNbt("actorprefix: ", data.data(), data.size(), actor_list).second[0].dump(4, ' ', false, nlohmann::detail::error_handler_t::ignore));
		}

		for (auto& village_id : villages) {
			std::string data;
			NbtTagList tags_info, tags_player, tags_dweller, tags_poi;
			leveldb::ReadOptions read_options;
			db->Get(read_options, ("VILLAGE_" + village_id + "_INFO"), &data);
			result = ParseNbt(data.data(), data.size(), tags_info).first;

			if (result != 0) continue;

			db->Get(read_options, ("VILLAGE_" + village_id + "_PLAYERS"), &data);
			result = ParseNbt(data.data(), data.size(), tags_player).first;

			if (result != 0) continue;

			db->Get(read_options, ("VILLAGE_" + village_id + "_DWELLERS"), &data);
			result = ParseNbt(data.data(), data.size(), tags_dweller).first;

			if (result != 0) continue;

			db->Get(read_options, ("VILLAGE_" + village_id + "_POI"), &data);
			result = ParseNbt(data.data(), data.size(), tags_poi).first;

			if (result != 0) continue;

			ParseNbtVillage(tags_info, tags_player, tags_dweller, tags_poi);
		}
		*/

		return 0;
	}

	int MinecraftWorldLevelDB::ParseDBKey(int x, int z) {
		std::string temp_data;
		for (int y = -64; y < 320; y += 16) {
			std::vector<char> bytes = CoordsToDbKey(x, z, y);
			std::string temp_str(bytes.begin(), bytes.end());

			db->Get(leveldb_read_options, leveldb::Slice(temp_str), &temp_data);

			leveldb::Slice key, value;
			size_t key_size, value_size;
			const char* key_name;
			const char* key_data;

			key = leveldb::Slice(temp_str);
			key_size = (int)key.size();
			key_name = key.data();
			value = leveldb::Slice(temp_data);
			value_size = value.size();
			key_data = value.data();

			if (IsChunkKey({ key_name,key_size }).first)
				ParseChunkKey({ key_name, key_size }, key_data, value_size);
		}

		return 0;
	}

	void MinecraftWorldLevelDB::ParseChunkKey(std::string_view key, const char* data, size_t size) {
		ChunkData chunk_data;

		chunk_data.chunk_x = key[0];
		chunk_data.chunk_z = key[4];
		chunk_data.chunk_type_sub = 0;

		switch (key.size()) {
		case 9: {
			chunk_data.chunk_dimension_id = 0;
			chunk_data.dimension_name = "overworld";
			chunk_data.chunk_tag = (ChunkTag)key[8];
		}
			  break;
		case 10: {
			chunk_data.chunk_dimension_id = 0;
			chunk_data.dimension_name = "overworld";
			chunk_data.chunk_tag = (ChunkTag)key[8];
			chunk_data.chunk_type_sub = key[9];
		}
			   break;
		case 13: {
			chunk_data.chunk_dimension_id = key[8];
			chunk_data.dimension_name = "nether";
			chunk_data.chunk_tag = (ChunkTag)key[12];

			if (chunk_data.chunk_dimension_id == 0x32373639)
				chunk_data.chunk_dimension_id = 2;

			if (chunk_data.chunk_dimension_id == 0x33373639)
				chunk_data.chunk_dimension_id = 1;

			// check for new dim id's
			if (chunk_data.chunk_dimension_id != 1 && chunk_data.chunk_dimension_id != 2)
				smokey_bedrock_parser::log::warn("UNKNOWN -- Found new chunk dimension id=0x{:x} -- Did Bedrock finally get custom dimensions? Or did Mojang add a new dimension?", chunk_data.chunk_dimension_id);
		}
			   break;
		case 14: {
			chunk_data.chunk_dimension_id = key[8];
			chunk_data.dimension_name = "nether";
			chunk_data.chunk_tag = (ChunkTag)key[12];
			chunk_data.chunk_type_sub = key[13];

			if (chunk_data.chunk_dimension_id == 0x32373639)
				chunk_data.chunk_dimension_id = 2;

			if (chunk_data.chunk_dimension_id == 0x33373639)
				chunk_data.chunk_dimension_id = 1;

			// check for new dim id's
			if (chunk_data.chunk_dimension_id != 1 && chunk_data.chunk_dimension_id != 2)
				smokey_bedrock_parser::log::warn("UNKNOWN -- Found new chunk dimension id=0x{:x} -- Did Bedrock finally get custom dimensions? Or did Mojang add a new dimension?", chunk_data.chunk_dimension_id);
		}
			   break;
		default:
			break;
		}

		log::info("{}-chunk: x:{} z:{} y:{} (type={})", chunk_data.dimension_name, chunk_data.chunk_x, chunk_data.chunk_z, chunk_data.chunk_type_sub, chunk_data.chunk_tag);

		switch (chunk_data.chunk_tag) {
		case ChunkTag::SubChunkPrefix: {
			if (data[0] != 0)
				dimensions[chunk_data.chunk_dimension_id]->AddChunk(7, chunk_data.chunk_x, chunk_data.chunk_type_sub, chunk_data.chunk_z, data, size);
		}
									 break;
		default:
			break;
		}
	}

	std::unique_ptr<MinecraftWorldLevelDB> world;
} // namespace smokey_bedrock_parser