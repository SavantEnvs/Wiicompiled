// Dolphin GCPadNew.ini import: the INI section/key reader (ReadDolphinConfig takes a path, so
// the input is written to a per-process scratch file under $TMPDIR), then each control's
// expression is parsed as the runtime does on import.
#include "input_expr.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

static std::string g_path;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    const char* tmp = std::getenv("TMPDIR");
    std::string tmpl = std::string(tmp && *tmp ? tmp : "/tmp") + "/fuzz_dolphin_ini.XXXXXX";
    int fd = mkstemp(tmpl.data());
    if (fd < 0) std::abort();
    close(fd);
    g_path = tmpl;
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FILE* f = std::fopen(g_path.c_str(), "wb");
    if (!f) std::abort();
    if (size) std::fwrite(data, 1, size, f);
    std::fclose(f);
    for (int pad = 1; pad <= 4; ++pad) {
        std::vector<std::pair<std::string, std::string>> controls;
        std::string device, error;
        if (!InputExpr::ReadDolphinConfig(g_path, pad, controls, device, error)) continue;
        for (const auto& control : controls) {
            InputExpr::Expression expr;
            std::string perr;
            if (InputExpr::Expression::Parse(control.second, expr, perr)) {
                (void)expr.Evaluate([](const std::string&) { return 1.0; });
            }
        }
    }
    return 0;
}
