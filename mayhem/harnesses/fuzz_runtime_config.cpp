// Config.toml (user-editable runtime settings): the toml11 parser plus every section of
// RuntimeConfigFile::ParseConfigDocument, then the in-place key rewrite the settings UI does.
#include "runtime_config.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    // ParseConfig logs every rejected document to std::cerr; silence it so the fuzzer's output
    // stays readable. Set once, never changed by an input.
    std::cerr.setstate(std::ios::failbit);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const std::string text(reinterpret_cast<const char*>(data), size);
    std::istringstream input(text);
    const RuntimeUserConfig config = RuntimeConfigFile::ParseConfig(input, "Config.toml");
    (void)config;
    (void)RuntimeConfigFile::UpdateSettingText(text, "video", "vsync", "true");
    (void)RuntimeConfigFile::UpdateSettingText(text, "audio", "volume", "0.5");
    return 0;
}
