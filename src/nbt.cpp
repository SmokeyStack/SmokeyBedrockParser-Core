#include "SmokeyBedrockParser-Core/nbt.h"

#include <tuple>

#include "SmokeyBedrockParser-Core/json/json.hpp"
#include "SmokeyBedrockParser-Core/logger.h"

int32_t global_nbt_list_number = 0;
int32_t global_nbt_compound_number = 0;

namespace smokey_bedrock_parser {
	static nlohmann::json::object_t ParseNbtTag(const NbtTag& tag) {
		nlohmann::json::object_t json;

		nbt::tag_type nbt_type = tag.second->get_type();

		switch (nbt_type) {
		case nbt::tag_type::End:
			log::trace("TAG_END");

			break;
		case nbt::tag_type::Byte: {
			nbt::tag_byte value = tag.second->as<nbt::tag_byte>();
			log::trace("TAG_BYTE: {}", value.get());
			json[tag.first] = value.get();
		}
								break;
		case nbt::tag_type::Short: {
			nbt::tag_short value = tag.second->as<nbt::tag_short>();
			log::trace("TAG_SHORT: {}", value.get());
			json[tag.first] = value.get();
		}
								 break;
		case nbt::tag_type::Int: {
			nbt::tag_int value = tag.second->as<nbt::tag_int>();
			log::trace("TAG_INT: {}", value.get());
			json[tag.first] = value.get();
		}
							   break;
		case nbt::tag_type::Long: {
			nbt::tag_long value = tag.second->as<nbt::tag_long>();
			log::trace("TAG_LONG: {}", value.get());
			json[tag.first] = value.get();
		}
								break;
		case nbt::tag_type::Float: {
			nbt::tag_float value = tag.second->as<nbt::tag_float>();
			log::trace("TAG_FLOAT: {}", value.get());
			json[tag.first] = value.get();
		}
								 break;
		case nbt::tag_type::Double: {
			nbt::tag_double value = tag.second->as<nbt::tag_double>();
			log::trace("TAG_DOUBLE: {}", value.get());
			json[tag.first] = value.get();
		}
								  break;
		case nbt::tag_type::Byte_Array:
			break;
		case nbt::tag_type::String: {
			nbt::tag_string value = tag.second->as<nbt::tag_string>();
			log::trace("TAG_STRING: {}", value.get());
			json[tag.first] = value.get();
		}
								  break;
		case nbt::tag_type::List: {
			nbt::tag_list value = tag.second->as<nbt::tag_list>();
			int32_t list_number = global_nbt_compound_number++;
			log::trace("LIST-{} {{", list_number);

			for (const auto& nbt_tag : value)
				json[tag.first].update(ParseNbtTag(std::make_pair(std::string(""), nbt_tag.get().clone())));
		}
								break;
		case nbt::tag_type::Compound: {
			nbt::tag_compound value = tag.second->as<nbt::tag_compound>();
			int32_t compound_number = global_nbt_compound_number++;
			log::trace("TAG_COMPOUND: {} ({} tags)", compound_number, value.size());

			for (const auto& nbt_tag : value)
				json[tag.first].update(ParseNbtTag(std::make_pair(nbt_tag.first, nbt_tag.second.get().clone())));

		}
									break;
		case nbt::tag_type::Int_Array:
			break;
		case nbt::tag_type::Long_Array:
			break;
		default:
			break;
		}

		return json;
	}

	std::pair<int, nlohmann::json> ParseNbt(const char* buffer, int32_t buffer_length, NbtTagList& tag_list) {
		global_nbt_list_number = 0;
		global_nbt_compound_number = 0;
		std::istringstream iss(std::string(buffer, buffer_length));
		nbt::io::stream_reader reader(iss, endian::little);
		tag_list.clear();
		bool done = false;
		std::istream& stream = reader.get_istr();

		while (!done && stream && !stream.eof()) {
			try {
				tag_list.push_back(reader.read_tag());
			}
			catch (const std::exception&) {
				done = true;
			}
		}

		NbtJson result;

		for (const auto& nbt_tag : tag_list)
			result.push_back(ParseNbtTag(nbt_tag));

		return std::make_pair(0, result);
	}

	// https://minecraft.wiki/w/Bedrock_Edition_level_format/Other_data_format#VILLAGE_[0-9a-f\\-]+_INFO
	int VillageInfo::set_village_info(nbt::tag_compound& tag) {
		x0 = tag["X0"].as<nbt::tag_int>().get();
		x1 = tag["X1"].as<nbt::tag_int>().get();
		y0 = tag["Y0"].as<nbt::tag_int>().get();
		y1 = tag["Y1"].as<nbt::tag_int>().get();
		z0 = tag["Z0"].as<nbt::tag_int>().get();
		z1 = tag["Z1"].as<nbt::tag_int>().get();
		position = std::make_tuple((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2);
		village_payload["Village Info"] = { {"Village Center", {{"x", std::get<0>(position)},{"y", std::get<1>(position)},{"z", std::get<2>(position)}}} };

		return 0;
	}

	// https://minecraft.wiki/w/Bedrock_Edition_level_format/Other_data_format#VILLAGE_[0-9a-f\\-]+_PLAYERS
	int VillageInfo::set_players(nbt::tag_compound& tag) {
		if (tag.has_key("Player", nbt::tag_type::List)) {
			nbt::tag_list player_list = tag["Player"].as<nbt::tag_list>();

			for (const auto& player : player_list) {
				nbt::tag_compound player_compound = player.as<nbt::tag_compound>();
				players[player_compound["ID"].as<nbt::tag_long>().get()] = player_compound["S"].as<nbt::tag_int>().get();
			}
		}

		return 0;
	}

	/*
	* https://minecraft.wiki/w/Bedrock_Edition_level_format/Other_data_format#VILLAGE_[0-9a-f\\-]+_DWELLERS
	* What is TS??? Edit: Timestamp according to Github Copilot.
	* There are four types of dwellers: villagers, iron golems, raiders, and cats. Each dweller has a unique ID and a timestamp (TS).
	* Issue is that Dwellers is a list so I do not know how to get the type of dweller.
	* TODO: Figure out how to get the type of dweller.
	*/
	int VillageInfo::set_dwellers(nbt::tag_compound& tag) {
		if (tag.has_key("Dwellers", nbt::tag_type::List)) {
			nbt::tag_list dweller_list = tag["Dwellers"].as<nbt::tag_list>();

			for (const auto& dweller : dweller_list) {
				for (const auto& dweller_compound : dweller.as<nbt::tag_compound>()) {
					nbt::tag_list dweller_tag = dweller_compound.second.as<nbt::tag_list>();
					for (const auto& value : dweller_tag) {
						nbt::tag_compound value_compound = value.as<nbt::tag_compound>();
						nbt::tag_list last_saved_pos = value_compound["last_saved_pos"].as<nbt::tag_list>();
						dwellers.push_back(std::make_tuple(last_saved_pos[0].as<nbt::tag_int>().get(), last_saved_pos[1].as<nbt::tag_int>().get(), last_saved_pos[2].as<nbt::tag_int>().get()));
						village_payload["Village Dwellers"].push_back({ { "ID", value_compound["ID"].as<nbt::tag_long>().get() }, {"Timestamp", value_compound["TS"].as<nbt::tag_long>().get()}, {"Last Saved Position", {last_saved_pos[0].as<nbt::tag_int>().get(), last_saved_pos[1].as<nbt::tag_int>().get(), last_saved_pos[2].as<nbt::tag_int>().get()}} });
					}
				}
			}
		}

		return 0;
	}

	// https://minecraft.wiki/w/Bedrock_Edition_level_format/Other_data_format#VILLAGE_[0-9a-f\\-]+_POI
	int VillageInfo::set_pois(nbt::tag_compound& tag) {
		int64_t id, capacity, owner_count, radius, weight;
		int32_t x, y, z, type;
		std::string init_event, name, sound_event;
		bool use_aabb;

		if (tag.has_key("POI", nbt::tag_type::List)) {
			nbt::tag_list poi_list = tag["POI"].as<nbt::tag_list>();

			for (const auto& poi : poi_list) {
				for (const auto& poi_compound : poi.as<nbt::tag_compound>()) {
					if (poi_compound.second.get_type() == nbt::tag_type::List) {
						nbt::tag_list poi_tag = poi_compound.second.as<nbt::tag_list>();

						for (const auto& value : poi_tag) {
							nbt::tag_compound value_compound = value.as<nbt::tag_compound>();

							if (!value_compound["Skip"].as<nbt::tag_byte>().get()) {
								capacity = value_compound["Capacity"].as<nbt::tag_long>().get();
								init_event = value_compound["InitEvent"].as<nbt::tag_string>().get();
								name = value_compound["Name"].as<nbt::tag_string>().get();
								owner_count = value_compound["OwnerCount"].as<nbt::tag_long>().get();
								radius = value_compound["Radius"].as<nbt::tag_float>().get();
								sound_event = value_compound["SoundEvent"].as<nbt::tag_string>().get();
								type = value_compound["Type"].as<nbt::tag_int>().get();
								use_aabb = value_compound["UseAABB"].as<nbt::tag_byte>().get();
								weight = value_compound["Weight"].as<nbt::tag_long>().get();
								x = value_compound["X"].as<nbt::tag_int>().get();
								y = value_compound["Y"].as<nbt::tag_int>().get();
								z = value_compound["Z"].as<nbt::tag_int>().get();
							}
							else
								log::info("We are skipping this POI.");
						}
					}
					else
						id = poi_compound.second.as<nbt::tag_long>().get();
				}

				village_payload["Village POIs"].push_back(
					{ { "VillagerID", id },
					{"Position", {{"x", x},{"y", y},{"z", z}}},
					{"Capacity", capacity},
					{"InitEvent", init_event},{"Name",name},
					{"OwnerCount", owner_count},
					{"Radius", radius},
					{"SoundEvent", sound_event},
					{"Type", type},
					{"UseAABB", use_aabb},
					{"Weight", weight} }
				);
			}
		}

		return 0;
	}

	void VillageInfo::print_info() {
		log::info("{}", village_payload.dump(4, ' ', false, nlohmann::detail::error_handler_t::ignore));
	}


	int ParseNbtVillage(NbtTagList& tags_info, NbtTagList& tags_player, NbtTagList& tags_dweller, NbtTagList& tags_poi) {
		std::unique_ptr<VillageInfo> village_info = std::make_unique<VillageInfo>();

		village_info->clear();
		village_info->set_village_info(tags_info[0].second->as<nbt::tag_compound>());
		village_info->set_players(tags_player[0].second->as<nbt::tag_compound>());
		village_info->set_dwellers(tags_dweller[0].second->as<nbt::tag_compound>());
		village_info->set_pois(tags_poi[0].second->as<nbt::tag_compound>());
		village_info->print_info();

		return 0;
	}
} // namespace smokey_bedrock_parser