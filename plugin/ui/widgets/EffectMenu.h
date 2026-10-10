// Add-effect browser: a two-column popover — vendors on the left, that vendor's
// plugins on the right. When a vendor is picked the plugin column opens and the
// vendor column is blurred (backdrop blur), so the "background" menu recedes.
// Drawn with our own components (not a native PopupMenu) so we can blur it.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

#include "MenuRow.h"
#include "Popover.h"

namespace t3k::ui {

class EffectMenu : public Popover {
public:
  struct Entry {
    juce::String vendor;
    juce::String name;
    juce::String format;  // "AU" or "VST3"
    juce::String path;
  };

  explicit EffectMenu(std::vector<Entry> entries);
  ~EffectMenu() override;

  // Chosen bundle path (called before the menu dismisses).
  std::function<void(const juce::String&)> onChoose;

  // Centre the panel over `anchor` (and keep it inside the overlay). Call after open().
  void centreOn(juce::Component& anchor);

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  // Paints a (blurred) snapshot of the vendor column over it.
  class Frost : public juce::Component {
  public:
    void setImage(juce::Image image);
    void paint(juce::Graphics& g) override;

  private:
    juce::Image image_;
  };

  void buildVendors();
  void buildPlugins(const juce::String& vendor);
  void pickVendor(const juce::String& vendor);
  void placeClamped(int x, int y);

  std::vector<Entry> entries_;
  juce::Component leftColumn_, rightColumn_;
  juce::Viewport leftView_, rightView_;
  Frost frost_;
  bool rightShown_ = false;

  static constexpr int kPad = 8;
  static constexpr int kLeftW = 210;
  static constexpr int kRightW = 290;
  static constexpr int kHeight = 420;
  static constexpr int kRowH = MenuRow::kContext.height;
  static constexpr int kScrollW = 8;
  static constexpr int kBlurRadius = 9;
};

}  // namespace t3k::ui
