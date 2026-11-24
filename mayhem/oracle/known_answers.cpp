// Known-answer oracle for mayhem/test.sh: runs the fuzzed parsers on the committed seed
// inputs and prints what they decoded; test.sh diffs this against oracle/expected.txt.
#include "hle/riivolution_contract.h"
#include "input_expr.h"
#include "runtime_config.h"
#include "u8_archive.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

static std::string Slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), {});
}

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    const std::string dir = argv[1];

    auto disc = RiivolutionContract::ParseString(Slurp(dir + "/fuzz_riivolution/testsuite/mod.xml"));
    std::printf("riivolution parsed=%d\n", disc ? 1 : 0);
    if (disc) {
        std::printf("riivolution valid RMCE01=%d RSBE01=%d sections=%zu patches=%zu\n",
                    disc->IsValidForGame("RMCE01", std::nullopt, std::nullopt),
                    disc->IsValidForGame("RSBE01", std::nullopt, std::nullopt),
                    disc->sections.size(), disc->patches.size());
        auto config = RiivolutionContract::ParseConfigString(
            Slurp(dir + "/fuzz_riivolution/testsuite/config.xml"));
        std::printf("riivolution config parsed=%d options=%zu\n", config ? 1 : 0,
                    config ? config->options.size() : 0);
        if (config) RiivolutionContract::ApplyConfigDefaults(*disc, *config);
        for (const auto& p : disc->GeneratePatches("RMCE01")) {
            std::printf("patch id=%s root=%s files=%zu folders=%zu memory=%zu\n", p.id.c_str(),
                        p.root.c_str(), p.filePatches.size(), p.folderPatches.size(),
                        p.memoryPatches.size());
            for (const auto& f : p.filePatches)
                std::printf("  file disc=%s external=%s offset=%u length=%u\n", f.disc.c_str(),
                            f.external.c_str(), f.offset, f.length);
        }
        auto abs = RiivolutionContract::MakeAbsoluteFromRelative("/sd", "/sd/CTGP", "../x");
        std::printf("traversal rejected=%d\n", abs ? 0 : 1);
    }

    const std::string u8 = Slurp(dir + "/fuzz_u8_archive/testsuite/archive");
    const size_t nul = u8.find('\0');
    std::vector<uint8_t> archive(u8.begin() + nul + 1, u8.end()), out;
    const bool found = RuntimeU8::Extract(archive, "dir/sub/RFL_Res.dat", out);
    std::printf("u8 found=%d size=%zu", found, out.size());
    for (uint8_t b : out) std::printf(" %02x", b);
    std::printf(" missing=%d\n", RuntimeU8::Extract(archive, "nope", out));

    std::istringstream toml(Slurp(dir + "/fuzz_runtime_config/testsuite/Config.toml"));
    const RuntimeUserConfig cfg = RuntimeConfigFile::ParseConfig(toml, "Config.toml");
    std::printf("config widescreen=%d vsync=%d width=%u api=%s\n",
                cfg.widescreen.value_or(false) ? 1 : 0, cfg.vsync.value_or(true) ? 1 : 0,
                cfg.windowWidth.value_or(0), cfg.graphicsApi.value_or("-").c_str());
    const auto updated = RuntimeConfigFile::UpdateSettingText("[video]\nvsync = false\n", "video",
                                                              "vsync", "true");
    std::printf("config update=%s", updated ? updated->c_str() : "<none>\n");

    const char* exprs[] = {"`A` | `B`", "`A` & `B`", "!`B`", "(`A` + `B`) * 0.5", "`A` &"};
    for (const char* text : exprs) {
        InputExpr::Expression e;
        std::string err;
        const bool ok = InputExpr::Expression::Parse(text, e, err);
        std::printf("expr [%s] ok=%d value=%.3f\n", text, ok,
                    ok ? e.Evaluate([](const std::string& n) { return n == "A" ? 1.0 : 0.25; })
                       : -1.0);
    }

    std::vector<std::pair<std::string, std::string>> controls;
    std::string device, error;
    const bool ini = InputExpr::ReadDolphinConfig(dir + "/fuzz_dolphin_ini/testsuite/GCPadNew.ini",
                                                  1, controls, device, error);
    std::printf("dolphin ok=%d device=%s controls=%zu\n", ini, device.c_str(), controls.size());
    for (const auto& c : controls) std::printf("  %s = %s\n", c.first.c_str(), c.second.c_str());
    return 0;
}
