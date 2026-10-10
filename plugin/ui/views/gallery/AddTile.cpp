#include "AddTile.h"

#include "GalleryGeometry.h"
#include "core/Help.h"
#include "core/Icons.h"
#include "core/Paint.h"
#include "core/Theme.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace t3k::ui {

AddTile::Routing AddTile::routingFor(int index, int count) {
  if (count <= 1) return Routing::none;
  if (index == 0) return Routing::right;
  if (index == count - 1) return Routing::left;
  return Routing::both;
}

AddTile::AddTile(Services& services, std::string blockId, int size)
    : GalleryTile(services, std::move(blockId), size) {
  setHelpText(help::text(help::Key::addTile));
  setTitle("Add Tone");
}

void AddTile::setRouting(Routing routing) {
  if (routing_ == routing) return;
  routing_ = routing;
  repaint();
}

void AddTile::setCanPaste(bool canPaste) { canPaste_ = canPaste; }

void AddTile::open() {
  if (onAdd) onAdd(blockId());
}

std::vector<ContextMenu::Item> AddTile::menuItems() {
  std::vector<ContextMenu::Item> items{
      {"Installed AU/VST3 Effects", Icon::File, help::Key::loadFileTile,
       [this] { chooseInstalledEffect(); }},
      {"Load AU/VST3 Effect", Icon::File, help::Key::loadFileTile,
       [this] { chooseEffect(); }},
      {"Paste", Icon::ClipboardPaste, help::Key::pasteBlock,
       [this] { if (onPaste) onPaste(blockId()); }, /*disabled=*/!canPaste_},
  };
  for (auto& item : localLoadItems()) items.push_back(std::move(item));
  return items;
}

void AddTile::chooseInstalledEffect() {
  const auto listed = services().backend.listHostedEffects();
  const auto* array = listed.getArray();
  if (array == nullptr || array->isEmpty()) {
    services().toast.show("No AU/VST3 effects found");
    return;
  }
  auto descriptions = std::make_shared<std::vector<juce::String>>();
  juce::PopupMenu menu;
  for (const auto& effect : *array) {
    descriptions->push_back(effect["description"].toString());
    menu.addItem(static_cast<int>(descriptions->size()),
                 effect["name"].toString() + " (" + effect["format"].toString() + ")");
  }
  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                     [safe = juce::Component::SafePointer<AddTile>(this), descriptions](int choice) {
                       if (safe == nullptr || choice <= 0 ||
                           choice > static_cast<int>(descriptions->size()))
                         return;
                       const auto id = safe->services().backend.addHostedEffect(
                           (*descriptions)[static_cast<size_t>(choice - 1)], safe->blockId());
                       if (id.empty())
                         safe->services().toast.show("Could not load this effect");
                       else
                         safe->services().chain.refresh(true);
                     });
}

void AddTile::chooseEffect() {
  effectChooser_ = std::make_unique<juce::FileChooser>(
      "Choose an Audio Unit or VST3 effect",
      juce::File("/Library/Audio/Plug-Ins"), "*.component;*.vst3");
  effectChooser_->launchAsync(
      juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
      [safe = juce::Component::SafePointer<AddTile>(this)](const juce::FileChooser& chooser) {
        if (safe == nullptr) return;
        const auto file = chooser.getResult();
        if (file == juce::File()) return;
        const auto id = safe->services().backend.addHostedEffectFromFile(file, safe->blockId());
        if (id.empty())
          safe->services().toast.show("Could not load this AU/VST3 effect");
        else
          safe->services().chain.refresh(true);
      });
}

void AddTile::paint(juce::Graphics& g) {
  const auto box = getLocalBounds().toFloat();
  const bool armed = dropArmed();
  paint::fill(g, box, gallery::kTileCorner, theme::kSurfaceRaised);
  paint::dashedBorder(g, box, gallery::kTileCorner,
                      armed ? gallery::kFileDropBorder : gallery::kAddTileBorder,
                      gallery::kAddTileBorderWidth);

  if (armed) {
    const float s = gallery::kFileDropGlyphSize;
    Icons::draw(g, Icon::Upload, box.withSizeKeepingCentre(s, s), theme::kGray);
    return;
  }

  const int size = tileSize();
  const int icon = gallery::plusIconSize(size);
  // Routing lines are anchored inside the border (absolute children position
  // against the padding box), so the run to the plus ring is a border-width
  // shorter than measured from the tile's outer edge.
  if (!travelling() && routing_ != Routing::none) {
    const float run = size / 2.0f - icon / 2.0f + gallery::plusCircleInset(icon) -
                      gallery::kAddTileBorderWidth;
    const float y = box.getCentreY() - gallery::kLineWidth / 2;
    g.setColour(theme::kWhite);
    if (routing_ == Routing::left || routing_ == Routing::both)
      g.fillRect(juce::Rectangle<float>(gallery::kAddTileBorderWidth, y, run, gallery::kLineWidth));
    if (routing_ == Routing::right || routing_ == Routing::both)
      g.fillRect(juce::Rectangle<float>(box.getRight() - gallery::kAddTileBorderWidth - run, y, run,
                                        gallery::kLineWidth));
  }
  Icons::draw(g, Icon::PlusCircle, box.withSizeKeepingCentre(icon, icon), theme::kWhite,
              /*strokeWidth=*/1.0f);
}

}  // namespace t3k::ui
