// Dolphin-compatible input expressions (controller mappings copied from GCPadNew.ini):
// lexer, parser, evaluator and ReferencedInputs.
#include "input_expr.h"

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const std::string text(reinterpret_cast<const char*>(data), size);
    InputExpr::Expression expr;
    std::string error;
    if (!InputExpr::Expression::Parse(text, expr, error)) return 0;
    // Deterministic input values derived from the referenced name.
    const InputExpr::InputSource source = [](const std::string& name) {
        size_t h = 1469598103u;
        for (unsigned char c : name) h = (h ^ c) * 16777619u;
        return static_cast<double>(h % 5) / 4.0;
    };
    (void)expr.Evaluate(source);
    (void)expr.Evaluate(source);
    (void)expr.ReferencedInputs();
    return 0;
}
