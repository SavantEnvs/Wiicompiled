// The emulated Bluetooth controller: HCI commands and ACL data from the guest's Bluetooth
// stack, L2CAP signalling/HID channels and SDP, with input reports from attached remotes.
// Input: records [kind][len][len bytes]. kind%4: 0 HCI command, 1 ACL packet, 2 tick,
// 3 queue the bytes as an input report from a remote (slot = kind>>2).
#include "bluetooth/bt_controller.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace {
using Bytes = std::vector<uint8_t>;

struct FuzzHost : Bluetooth::RemoteHost {
    std::array<bool, Bluetooth::kRemoteSlots> attached{};
    std::array<bool, Bluetooth::kRemoteSlots> wake{};
    std::array<std::deque<Bytes>, Bluetooth::kRemoteSlots> input;

    bool IsAttached(size_t slot) const override { return attached[slot]; }
    bool ConsumeWake(size_t slot) override {
        const bool w = wake[slot];
        wake[slot] = false;
        return w;
    }
    bool PopInputReport(size_t slot, Bytes& report) override {
        if (input[slot].empty()) return false;
        report = std::move(input[slot].front());
        input[slot].pop_front();
        return true;
    }
    void SendOutputReport(size_t, const uint8_t*, size_t) override {}
    void OnLinkStateChanged(size_t, Bluetooth::LinkState) override {}
    void OnGuestDisconnect(size_t) override {}
    void OnGuestInquiry(bool) override {}
    const char* DeviceName(size_t) const override { return "Nintendo RVL-CNT-01"; }
};
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzHost host;
    host.attached[0] = true;
    host.wake[0] = true;
    Bluetooth::Controller controller(host);
    uint64_t now = 1000;
    controller.Open(now);

    Bytes out;
    size_t pos = 0;
    while (pos + 2 <= size) {
        const uint8_t kind = data[pos];
        size_t len = data[pos + 1];
        pos += 2;
        if (len > size - pos) len = size - pos;
        const uint8_t* body = data + pos;
        pos += len;
        switch (kind % 4) {
        case 0: controller.ExecuteCommand(body, len, now); break;
        case 1: controller.ReceiveAcl(body, len, now); break;
        case 2: now += 1000u * (1u + len); controller.Tick(now); break;
        default: {
            const size_t slot = (kind >> 2) % Bluetooth::kRemoteSlots;
            host.attached[slot] = true;
            host.wake[slot] = true;
            host.input[slot].emplace_back(body, body + len);
            break;
        }
        }
        while (controller.PopEvent(out)) {}
        while (controller.PopAcl(out)) {}
    }
    controller.Close();
    return 0;
}
