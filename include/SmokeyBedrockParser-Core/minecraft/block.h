#pragma once

#include <string>

namespace smokey_bedrock_parser {
    class Block {
    public:
        std::string name;

        Block(const Block&) = delete;
        void operator=(const Block&) = delete;

        static Block& GetInstance(const std::string& name);
        static Block* Add(const std::string& name);
        static Block* Get(const std::string& name);
    private:
        Block(const std::string& name) {
            this->name = name;
        }
    };
}