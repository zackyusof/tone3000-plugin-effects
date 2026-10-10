#include "EffectMenu.h"

#include <algorithm>

#include "core/Blur.h"
#include "core/Paint.h"
#include "core/Theme.h"

namespace t3k::ui {

void EffectMenu::Frost::setImage(juce::Image image) {
  image_ = std::move(image);
  repaint();
}

void EffectMenu::Frost::paint(juce::Graphics& g) {
  if (image_.isValid())
    g.drawImageAt(image_, 0, 0);
}

EffectMenu::EffectMenu(std::vector<Entry> entries) : entries_(std::move(entries)) {
  setTitle("Add effect");

  leftColumn_.addAndMakeVisible(leftView_);
  rightColumn_.addAndMakeVisible(rightView_);
  addAndMakeVisible(leftColumn_);
  addChildComponent(rightColumn_);
  addChildComponent(frost_);
  frost_.setInterceptsMouseClicks(false, false);

  leftView_.setScrollBarsShown(true, false);
  leftView_.setScrollBarThickness(kScrollW);
  rightView_.setScrollBarsShown(true, false);
  rightView_.setScrollBarThickness(kScrollW);

  buildVendors();
  setSize(2 * (kBorder + kPad) + kLeftW, 2 * (kBorder + kPad) + kHeight);
}

void EffectMenu::centreOn(juce::Component& anchor) {
  auto* overlay = getParentComponent();
  if (overlay == nullptr)
    return;
  // Never taller than the overlay (the panel is taller than most plugin windows).
  const int maxH = juce::jmax(160, overlay->getHeight() - 24);
  if (getHeight() > maxH)
    setSize(getWidth(), maxH);
  const auto a = overlay->getLocalArea(&anchor, anchor.getLocalBounds().toFloat());
  placeClamped(juce::roundToInt(a.getCentreX()) - getWidth() / 2,
               juce::roundToInt(a.getBottom()) + 8);
}

void EffectMenu::placeClamped(int x, int y) {
  auto* overlay = getParentComponent();
  if (overlay == nullptr)
    return;
  const int maxX = juce::jmax(0, overlay->getWidth() - getWidth());
  const int maxY = juce::jmax(0, overlay->getHeight() - getHeight());
  setTopLeftPosition(juce::jlimit(0, maxX, x), juce::jlimit(0, maxY, y));
}

EffectMenu::~EffectMenu() = default;

void EffectMenu::paint(juce::Graphics& g) {
  const auto box = getLocalBounds().toFloat();
  paint::fill(g, box, theme::kPanelCorner, theme::kPanelBg);
  paint::border(g, box, theme::kPanelCorner, theme::kBorder);
}

void EffectMenu::resized() {
  auto r = contentBounds().reduced(kPad);
  const int fullH = r.getHeight();
  leftColumn_.setBounds(r.removeFromLeft(kLeftW).withHeight(fullH));
  if (rightShown_)
    rightColumn_.setBounds(r.removeFromLeft(kRightW).withHeight(fullH));
  leftView_.setBounds(leftColumn_.getLocalBounds());
  rightView_.setBounds(rightColumn_.getLocalBounds());
  frost_.setBounds(leftColumn_.getBounds());
}

void EffectMenu::buildVendors() {
  juce::StringArray vendors;
  for (const auto& e : entries_)
    if (!vendors.contains(e.vendor))
      vendors.add(e.vendor);
  vendors.sort(true);

  auto* content = new juce::Component();
  const int w = kLeftW - kScrollW;
  content->setSize(w, juce::jmax(kRowH, vendors.size() * kRowH));
  int y = 0;
  for (const auto& vendor : vendors) {
    auto* row = new MenuRow(vendor, std::nullopt, MenuRow::kContext);
    row->setLabelColour(theme::kWhite);
    row->setBounds(0, y, w, kRowH);
    row->onClick = [this, vendor] { pickVendor(vendor); };
    content->addAndMakeVisible(row);
    y += kRowH;
  }
  leftView_.setViewedComponent(content, true);
  leftView_.setViewPosition(0, 0);
}

void EffectMenu::buildPlugins(const juce::String& vendor) {
  std::vector<const Entry*> items;
  for (const auto& e : entries_)
    if (e.vendor == vendor)
      items.push_back(&e);
  std::sort(items.begin(), items.end(), [](const Entry* a, const Entry* b) {
    return a->name.compareIgnoreCase(b->name) < 0;
  });

  auto* content = new juce::Component();
  const int w = kRightW - kScrollW;
  content->setSize(w, juce::jmax(kRowH, static_cast<int>(items.size()) * kRowH));
  int y = 0;
  for (size_t i = 0; i < items.size(); ++i) {
    const auto* entry = items[i];
    juce::String label = entry->name;
    for (size_t j = 0; j < items.size(); ++j)
      if (j != i && items[j]->name == entry->name) {
        label << " (" << entry->format << ")";
        break;
      }
    auto* row = new MenuRow(label, std::nullopt, MenuRow::kContext);
    row->setLabelColour(theme::kWhite);
    row->setBounds(0, y, w, kRowH);
    const auto path = entry->path;
    const auto name = entry->name;
    row->onClick = [this, path, name] {
      // Close the menu before handing off; the handler defers the actual work so
      // nothing tears this menu (or the tiles) down while the button is mid-click.
      dismiss();
      if (onChoose)
        onChoose(path, name);
    };
    content->addAndMakeVisible(row);
    y += kRowH;
  }
  rightView_.setViewedComponent(content, true);
  rightView_.setViewPosition(0, 0);
}

void EffectMenu::pickVendor(const juce::String& vendor) {
  buildPlugins(vendor);
  rightShown_ = true;
  const int centreX = getX() + getWidth() / 2;  // keep the panel centred as it widens
  setSize(2 * (kBorder + kPad) + kLeftW + kRightW, 2 * (kBorder + kPad) + kHeight);
  rightColumn_.setVisible(true);

  // Blur the (now in the background) vendor column. Snapshot before revealing the
  // plugin column so the blur does not capture the plugin rows.
  auto snapshot = leftColumn_.createComponentSnapshot(leftColumn_.getLocalBounds());
  blurImage(snapshot, kBlurRadius);
  frost_.setImage(std::move(snapshot));
  frost_.setVisible(true);
  frost_.toFront(false);
  rightColumn_.toFront(false);

  placeClamped(centreX - getWidth() / 2, getY());
}

}  // namespace t3k::ui
