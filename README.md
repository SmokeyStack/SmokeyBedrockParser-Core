# SmokeyBedrockParser-Core

SmokeyBedrockParser-Core is a C++ library for parsing Minecraft: Bedrock Edition worlds.

## Usage

SmokeyBedrockParser is C++ library that interacts with the leveldb databases that Minecraft: Bedrock Edition uses to store world data. It is designed to be used as a submodule in other projects.

## Building

- Install [Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
- Install [Vcpkg](https://github.com/microsoft/vcpkg) and set up command line integration as described in the Vcpkg docs. Remember the location of Vcpkg's `CMAKE_TOOLCHAIN_FILE`
- Run `vcpkg install zlib:x64-windows`
- Navigate to `compile.bat` and edit the path to Vcpkg's `CMAKE_TOOLCHAIN_FILE` to match your installation.
- Run `compile.bat` to build the project.
- If this is successful, SmokeyBedrockParser-Core can be found in `build/Release/SmokeyBedrockParser-Core.lib`.

## Third Party Software

This project uses the following third party software:

- [google/leveldb](https://github.com/google/leveldb)
- [ljfa-ag/libnbtplusplus](https://github.com/ljfa-ag/libnbtplusplus)
- [gabime/spdlog](https://github.com/gabime/spdlog)
- [nlohmann/json](https://github.com/nlohmann/json)