#include "SmokeyBedrockParser-Core/minecraft/block.h"

#include <vector>
#include <map>

#include "SmokeyBedrockParser-Core/logger.h"

namespace {
    std::map<std::string, smokey_bedrock_parser::Block*> block_map;
}

namespace smokey_bedrock_parser {
    Block& Block::GetInstance(const std::string& name) {
        static Block instance(name);

        return instance;
    }

    Block* Block::Add(const std::string& name) {
        auto block = new Block(name);

        if (block_map.find(name) != block_map.end())
            log::error("{} already exists", name);
        else
            block_map[name] = block;

        return block;
    }

    Block* Block::Get(const std::string& name) {
        auto it = block_map.find(name);

        if (it != block_map.end())
            return it->second;

        log::trace("Block: {} not found", name);
        return nullptr;
    }
} // namespace smokey_bedrock_parser