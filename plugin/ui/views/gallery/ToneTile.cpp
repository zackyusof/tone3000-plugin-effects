#include "ToneTile.h"

#include "GalleryGeometry.h"
#include "core/Help.h"
#include "core/Icons.h"
#include "core/Paint.h"
#include "core/Theme.h"
#include "widgets/HostedEffectWindow.h"

namespace t3k::ui {

namespace {
constexpr int kImageFadeMs = 200;
constexpr float kDimmedImage = 0.35f;
const juce::Colour kStrip = juce::Colours::black.withAlpha(0.35f);

// Vendor badge: a monogram on a colour derived from the vendor name (no offline
// vendor logos exist, and the generic macOS bundle icon is ugly).
juce::String vendorMonogram(const juce::String& vendor) {
  if (vendor.isEmpty()) return "?";
  const auto words = juce::StringArray::fromTokens(vendor, " -_", "");
  juce::String monogram;
  for (const auto& w : words) {
    if (w.isNotEmpty()) {
      monogram << w.substring(0, 1).toUpperCase();
      if (monogram.length() >= 2) break;
    }
  }
  return monogram.isNotEmpty() ? monogram : vendor.substring(0, 1).toUpperCase();
}

juce::Colour vendorColour(const juce::String& vendor) {
  const auto h = static_cast<juce::uint32>(vendor.hashCode());
  return juce::Colour::fromHSV((h % 360u) / 360.0f, 0.52f, 0.62f, 1.0f);
}
}  // namespace

void ToneTile::Strip::paint(juce::Graphics& g) {
  g.setColour(kStrip);
  g.fillRect(getLocalBounds().withHeight(kChromeHeight));
}

ToneTile::ToneTile(Services& services, const ChainItem& block, int size)
    : GalleryTile(services, block.blockId, size),
      block_(block),
      image_(services.images),
      power_(Icon::Power, ChromeIconButton::Tone::power, help::Key::blockPower),
      swap_(Icon::ArrowLeftRight, ChromeIconButton::Tone::plain, help::Key::swapTone),
      remove_(Icon::Trash2, ChromeIconButton::Tone::plain, help::Key::removeBlock),
      glow_(services.meters, MeterStore::blockOutId(block.blockId)),
      led_(services.meters, MeterStore::blockOutId(block.blockId)) {
  image_.setCornerRadius(gallery::kTileCorner);
  image_.setGlow(glow_.glow());
  glow_.onChange = [this] { image_.setGlow(glow_.glow()); };
  addAndMakeVisible(image_);
  addChildComponent(dots_);
  addChildComponent(retry_);
  retry_.onRetry = [this] { this->services().modelLoads.retry(blockId()); };

  addAndMakeVisible(chrome_);
  for (auto* b : {&power_, &swap_, &remove_}) chrome_.addAndMakeVisible(*b);
  power_.onClick = [this] { togglePower(); };
  swap_.onClick = [this] { if (onSwap) onSwap(blockId()); };
  remove_.onClick = [this] { this->services().chain.removeBlock(blockId()); };

  addAndMakeVisible(ledSlot_);
  ledSlot_.addChildComponent(led_);
  ledSlot_.setSize(BlockLed::kSize, BlockLed::kSize);
  addMouseListener(&hover_, true);
  services.pointer.addListener(this);
  hostedEffectGraphics().addChangeListener(this);  // repaint when a slot graphic arrives

  setBlock(block);
  setHovered(false);
  resized();  // the base set the size before these children existed
}

ToneTile::~ToneTile() {
  hostedEffectGraphics().removeChangeListener(this);
  services().pointer.removeListener(this);
  removeMouseListener(&hover_);
}

void ToneTile::changeListenerCallback(juce::ChangeBroadcaster*) { repaint(); }

void ToneTile::setBlock(const ChainItem& block) {
  block_ = block;
  enabled_ = block.params.enabled;
  setHelpText(help::toneTile(block.tone.title));
  setTitle(block.tone.title);
  image_.setTone(block.tone.image, block.tone.gear, block.tone.local, kGlyphSize);
  swap_.setVisible(!block.isEffect);
  syncState();
}

// A model download/prepare is in flight: `modelLoading` covers switches
// (the previous model keeps playing, so `loaded` stays true) and `!loaded`
// covers fresh blocks that have nothing to play yet.
void ToneTile::syncState() {
  const bool busy = block_.modelLoading || (!block_.loaded && !block_.loadFailed);
  const bool armed = dropArmed();
  image_.setVisible(!armed && !block_.isEffect);
  imageFade_.animateTo(enabled_ && !busy && !block_.loadFailed ? 1.0f : kDimmedImage, kImageFadeMs);
  dots_.setVisible(!armed && busy && !block_.loadFailed);
  retry_.setVisible(!armed && block_.loadFailed);
  chrome_.setVisible(!armed);
  ledSlot_.setVisible(!armed);
  power_.setOn(enabled_);
  repaint();
}

void ToneTile::togglePower() {
  enabled_ = !enabled_;
  power_.setOn(enabled_);
  imageFade_.animateTo(enabled_ ? 1.0f : kDimmedImage, kImageFadeMs);
  this->services().chain.setBlockParam(blockId(), "enabled", enabled_);
}

void ToneTile::open() {
  if (block_.isEffect) {
    if (!openHostedEffectEditor(services(), blockId(), block_.tone.title))
      services().toast.show("Effect is missing; reinstall it to restore this block");
    return;
  }
  if (onOpen) onOpen(blockId());
}

std::vector<ContextMenu::Item> ToneTile::menuItems() {
  if (block_.isEffect)
    return {{"Open Effect Editor", Icon::File, help::Key::loadFileTile,
             [this] { open(); }},
            {"Remove", Icon::Trash2, help::Key::removeBlock,
             [this] { services().chain.removeBlock(blockId()); }}};
  std::vector<ContextMenu::Item> items{
      {"Copy", Icon::Copy, help::Key::copyBlock,
       [this] { this->services().chain.copyBlock(blockId()); }},
  };
  for (auto& item : localLoadItems()) items.push_back(std::move(item));
  return items;
}

void ToneTile::dropArmedChanged(bool) { syncState(); }

void ToneTile::travellingChanged(bool) { setHovered(hovered_); }

void ToneTile::pointerMoved(const juce::MouseEvent& e, bool leaving) {
  // Leaving onto one of our own children is still hovering the tile.
  setHovered(!leaving || getLocalBounds().contains(e.getEventRelativeTo(this).getPosition()));
}

void ToneTile::setHovered(bool hovered) {
  hovered_ = hovered;
  const bool shown = services().pointer.coarse() || hovered_ || travelling();
  chrome_.setAlpha(shown ? 1.0f : 0.0f);
  chrome_.setInterceptsMouseClicks(false, shown);
}

void ToneTile::resized() {
  const auto box = getLocalBounds();
  image_.setBounds(box);
  dots_.setBounds(box.withSizeKeepingCentre(LoadingDots::kWidth, LoadingDots::kDot + 2 * LoadingDots::kMargin));
  retry_.setBounds(box.withSizeKeepingCentre(retry_.getWidth(), retry_.getHeight()));

  chrome_.setBounds(box.withHeight(kChromeHeight));
  const int y = kChromePad;
  power_.setBounds(kChromePad, y, theme::kIconBoxSize, theme::kIconBoxSize);
  remove_.setBounds(getWidth() - kChromePad - theme::kIconBoxSize, y, theme::kIconBoxSize,
                    theme::kIconBoxSize);
  swap_.setBounds(remove_.getX() - kChromeGap - theme::kIconBoxSize, y, theme::kIconBoxSize,
                  theme::kIconBoxSize);

  ledSlot_.setTopLeftPosition(getWidth() - kLedInset - BlockLed::kSize,
                              getHeight() - kLedInset - BlockLed::kSize);
}

void ToneTile::paint(juce::Graphics& g) {
  paint::fill(g, getLocalBounds().toFloat(), gallery::kTileCorner, theme::kSurface);
  if (block_.isEffect && !dropArmed()) {
    auto area = getLocalBounds().reduced(12);
    auto nameArea = area.removeFromBottom(18);
    const auto graphic = hostedEffectGraphic(blockId());
    if (graphic.isValid()) {
      // The plugin's own interface, captured when its window was open.
      g.drawImage(graphic, area.toFloat(),
                  juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    } else {
      const auto vendor = juce::String(block_.effectVendor);
      const int d = juce::jmin(area.getWidth(), area.getHeight());
      const auto badge = juce::Rectangle<int>(d, d).withCentre(area.getCentre()).toFloat();
      g.setColour(vendorColour(vendor));
      g.fillRoundedRectangle(badge, d * 0.22f);
      g.setColour(juce::Colours::white);
      g.setFont(juce::Font(juce::FontOptions(d * 0.42f, juce::Font::bold)));
      g.drawFittedText(vendorMonogram(vendor), badge.toNearestInt(), juce::Justification::centred, 1);
    }
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawFittedText(block_.tone.title, nameArea, juce::Justification::centred, 1);
  }
  if (dropArmed()) {
    const float s = gallery::kFileDropGlyphSize;
    Icons::draw(g, Icon::Upload, getLocalBounds().toFloat().withSizeKeepingCentre(s, s), theme::kGray);
  }
}

void ToneTile::paintOverChildren(juce::Graphics& g) {
  if (dropArmed())
    paint::dashedBorder(g, getLocalBounds().toFloat(), gallery::kTileCorner,
                        gallery::kFileDropBorder, gallery::kAddTileBorderWidth);
}

}  // namespace t3k::ui
