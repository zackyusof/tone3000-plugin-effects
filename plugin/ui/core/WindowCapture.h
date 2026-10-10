// Captures the on-screen pixels of a component's window. A JUCE component
// snapshot renders nothing for plugins whose editor is a native (Metal/AppKit)
// view, so effect-slot thumbnails use this instead. macOS only.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace t3k::ui {

// Empty when the component has no peer/window or the capture fails.
juce::Image captureComponentWindow(juce::Component& component);

}  // namespace t3k::ui
