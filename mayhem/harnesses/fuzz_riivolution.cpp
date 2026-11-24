// Riivolution mod XML (riivolution/*.xml, downloaded mod packs) and its remembered-choice
// config XML (riivolution/config/<GameID4>.xml): parse, match against the game, apply the
// config's defaults, and generate the patch list, as the runtime's mod loader does.
#include "hle/riivolution_contract.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const std::string_view xml(reinterpret_cast<const char*>(data), size);

    if (auto config = RiivolutionContract::ParseConfigString(xml)) {
        (void)config->options.size();
    }

    auto disc = RiivolutionContract::ParseString(xml);
    if (!disc) return 0;

    (void)disc->IsValidForGame("RMCE01", std::nullopt, std::nullopt);
    (void)disc->IsValidForGame("RMCP01", static_cast<uint16_t>(1), static_cast<uint8_t>(0));

    if (auto config = RiivolutionContract::ParseConfigString(xml)) {
        RiivolutionContract::ApplyConfigDefaults(*disc, *config);
    }

    std::vector<RiivolutionContract::OptionSelection> selections;
    for (const auto& section : disc->sections) {
        for (const auto& option : section.options) {
            selections.push_back({section.name, option.id, 1});
        }
    }
    RiivolutionContract::ApplySelections(*disc, selections);

    const auto patches = disc->GeneratePatches("RMCE01");
    for (const auto& patch : patches) {
        for (const auto& file : patch.filePatches) {
            const std::string root =
                RiivolutionContract::ResolvePatchRoot("/sd", "/sd/riivolution", patch.root);
            (void)RiivolutionContract::MakeAbsoluteFromRelative("/sd", root, file.external);
        }
        for (const auto& folder : patch.folderPatches) {
            const std::string root =
                RiivolutionContract::ResolvePatchRoot("/sd", "/sd/riivolution", patch.root);
            (void)RiivolutionContract::MakeAbsoluteFromRelative("/sd", root, folder.external);
        }
    }
    (void)RiivolutionContract::FindSavegamePatch(patches);
    return 0;
}
