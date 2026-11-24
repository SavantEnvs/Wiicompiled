// HID reports exchanged with a real (or spoofed) Bluetooth Wii Remote: extension register
// writes/reads, key exchange + decryption, calibration, and neutralising a data report.
// Input: a sequence of records [kind][len][len bytes]; kind%3 picks ObserveOutput (host->remote),
// ObserveInput (remote->host) or Neutralize.
#include "wiimote_extension.h"

#include <cstddef>
#include <cstdint>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    Wiimote::ExtensionState state;
    size_t pos = 0;
    while (pos + 2 <= size) {
        const uint8_t kind = data[pos];
        size_t len = data[pos + 1];
        pos += 2;
        if (len > size - pos) len = size - pos;
        std::vector<uint8_t> report(data + pos, data + pos + len);
        pos += len;
        switch (kind % 3) {
        case 0: state.ObserveOutput(report.data(), report.size()); break;
        case 1: state.ObserveInput(report.data(), report.size()); break;
        default: state.Neutralize(report); break;
        }
    }
    return 0;
}
