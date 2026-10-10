// Shows a hosted AU/VST3 effect's own editor GUI (the plugin's graphics) in a
// top-level window. Shared by the effect tile (click) and the add-effect flow
// (auto-open once the effect has loaded).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <string>

#include "services/Services.h"

namespace t3k::ui {

// Opens `blockId`'s hosted effect editor. Returns false when the block has no
// loaded instance (e.g. its plugin is missing) — the caller shows a message.
bool openHostedEffectEditor(Services& services, const std::string& blockId,
                            const juce::String& name);

}  // namespace t3k::ui
