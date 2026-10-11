#pragma once
// Fork-only: bring-your-own TONE3000 API key.
//
// Public builds ship WITHOUT a baked publishable key (see
// cmake/ForkIdentity.cmake and the release workflow): every user pastes their
// own free key — generated at tone3000.com/settings after signing in — into
// Plugin Settings, and all catalog traffic runs under that key. A key saved
// here wins over the baked build key (which may be empty); clearing it falls
// back to the baked key. Changing the key drops the stored tokens and cached
// identity (they belong to the previous key), so the next sign-in starts
// clean. Takes effect when the editor is (re)opened: the session reads it at
// construction.
#include <juce_core/juce_core.h>

#include "UiPrefs.h"

namespace t3k::ui::fork {

inline constexpr const char* kCustomPublishableKey = "fork.customPublishableKey";

inline juce::String customPublishableKey(UiPrefs& prefs) { return prefs.get(kCustomPublishableKey).trim(); }

inline void setCustomPublishableKey(UiPrefs& prefs, const juce::String& key) {
  const auto value = key.trim();
  if (value == customPublishableKey(prefs)) return;
  if (value.isEmpty())
    prefs.remove(kCustomPublishableKey);
  else
    prefs.set(kCustomPublishableKey, value);
  // Tokens and the cached identity were issued under the previous key.
  prefs.remove(UiPrefs::kTokens);
  prefs.remove(UiPrefs::kCachedUser);
}

}  // namespace t3k::ui::fork
