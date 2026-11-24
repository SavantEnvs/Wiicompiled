// U8 (.arc) archives read from NAND/banner data: the node table walk and file extraction.
// Input: <target name>\0<archive bytes>; with no NUL the whole input is the archive and a
// fixed set of names (the ones the runtime looks up) is tried.
#include "u8_archive.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const uint8_t* nul = static_cast<const uint8_t*>(std::memchr(data, 0, size < 256 ? size : 256));
    std::string name;
    std::vector<uint8_t> archive;
    if (nul) {
        name.assign(reinterpret_cast<const char*>(data), static_cast<size_t>(nul - data));
        archive.assign(nul + 1, data + size);
    } else {
        archive.assign(data, data + size);
    }
    std::vector<uint8_t> out;
    if (!name.empty()) (void)RuntimeU8::Extract(archive, name, out);
    (void)RuntimeU8::Extract(archive, "RFL_Res.dat", out);
    (void)RuntimeU8::Extract(archive, "dir/sub/RFL_Res.dat", out);
    (void)RuntimeU8::Extract(archive, "banner.bin", out);
    return 0;
}
