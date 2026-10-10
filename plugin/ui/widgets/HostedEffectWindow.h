// Hosts a hosted AU/VST3 effect's own editor GUI in a top-level window, and
// captures a static image of that GUI for the effect's chain slot.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

#include <string>

#include "services/Services.h"

namespace t3k::ui {

// Opens `blockId`'s hosted effect editor. Returns false when the block has no
// loaded instance (e.g. its plugin is missing) — the caller shows a message.
bool openHostedEffectEditor(Services& services, const std::string& blockId,
                            const juce::String& name);

// Static graphic for an effect block: the plugin's own GUI captured while its
// editor window was open (so the slot can show what's loaded). Empty until the
// editor has been opened at least once.
juce::Image hostedEffectGraphic(const std::string& blockId);

// Broadcasts when a graphic becomes available, so slots repaint themselves.
juce::ChangeBroadcaster& hostedEffectGraphics();

}  // namespace t3k::ui
