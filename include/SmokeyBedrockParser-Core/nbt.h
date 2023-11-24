#pragma once

#include "nbt_tags.h"
#include "SmokeyBedrockParser-Core/json/json.hpp"

namespace smokey_bedrock_parser {
	typedef  nlohmann::json NbtJson;
	typedef std::pair<std::string, std::unique_ptr<nbt::tag>> NbtTag;
	typedef std::vector<NbtTag> NbtTagList;

	class PlayerInfo {
	public:
		int64_t unique_id;
		std::string player_id;
	};

	class VillageInfo {
	public:
		VillageInfo() {
			clear();
		}

		void clear() {
			players.clear();
			x0 = x1 = y0 = y1 = z0 = z1 = 0;
		}

		int set_village_info(nbt::tag_compound& tag);

		int set_players(nbt::tag_compound& tag);

		int set_dwellers(nbt::tag_compound& tag);

		int set_pois(nbt::tag_compound& tag);

		void print_info();
	private:
		nlohmann::json village_payload;
		std::tuple<int32_t, int32_t, int32_t> position;
		int32_t x0, x1, y0, y1, z0, z1;
		std::map<int64_t, int32_t> players;
		std::vector<std::tuple<int32_t, int32_t, int32_t>> dwellers;
		std::vector<std::tuple<int32_t, int32_t, int32_t>> villagers;
		std::vector<std::tuple<int32_t, int32_t, int32_t>> golems;
		std::vector<std::tuple<int32_t, int32_t, int32_t>> raiders;
		std::vector<std::tuple<int32_t, int32_t, int32_t>> cats;
	};

	std::pair<int, nlohmann::json> ParseNbt(const char* buffer, int32_t buffer_length, NbtTagList& tag_list);
	int ParseNbtVillage(NbtTagList& tags_info, NbtTagList& tags_player, NbtTagList& tags_dweller, NbtTagList& tags_poi);
} // namespace smokey_bedrock_parser