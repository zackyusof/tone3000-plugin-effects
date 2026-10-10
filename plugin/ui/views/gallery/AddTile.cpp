#include "AddTile.h"

#include "GalleryGeometry.h"
#include "core/Help.h"
#include "core/Icons.h"
#include "core/Paint.h"
#include "core/Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <map>

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
      {"Add AU/VST3 Effect", Icon::File, help::Key::loadFileTile,
       [this] { chooseInstalledEffect(); }},
      {"Paste", Icon::ClipboardPaste, help::Key::pasteBlock,
       [this] { if (onPaste) onPaste(blockId()); }, /*disabled=*/!canPaste_},
  };
  for (auto& item : localLoadItems()) items.push_back(std::move(item));
  return items;
}

void AddTile::chooseInstalledEffect() {
  // Build the menu from a plain directory listing of the two standard system
  // folders. This does NOT instantiate/scan the plugins, so it returns instantly
  // and never blocks the message thread. (JUCE's searchPathsForPlugins loads
  // every plugin to identify it — that is what hung the UI before.)
  const auto listed = services().backend.listHostedEffectBundles();
  const auto* array = listed.getArray();
  if (array == nullptr || array->isEmpty()) {
    services().toast.show("No AU/VST3 effects found in /Library/Audio/Plug-Ins");
    return;
  }

  struct Entry {
    juce::String name;
    juce::String path;
  };
  // Group by format, then by first letter. There can be 1000+ installed effects,
  // so a flat list is unusable; the letter submenus keep it navigable.
  std::map<juce::String, std::vector<Entry>> byFormat;
  for (const auto& effect : *array) {
    const auto key = effect["format"].toString() == "VST3" ? juce::String("VST3")
                                                           : juce::String("Audio Units");
    byFormat[key].push_back({effect["name"].toString(), effect["path"].toString()});
  }

  juce::PopupMenu menu;
  for (auto& formatPair : byFormat) {
    auto& items = formatPair.second;
    std::sort(items.begin(), items.end(), [](const Entry& a, const Entry& b) {
      return a.name.compareIgnoreCase(b.name) < 0;
    });

    std::map<juce::String, juce::PopupMenu> byLetter;
    for (const auto& entry : items) {
      auto letter = entry.name.substring(0, 1).toUpperCase();
      if (letter.isEmpty() || !letter.containsOnly("ABCDEFGHIJKLMNOPQRSTUVWXYZ"))
        letter = "#";
      byLetter[letter].addItem(
          entry.name, true, false,
          [safe = juce::Component::SafePointer<AddTile>(this), path = entry.path] {
            if (safe == nullptr) return;
            const auto id = safe->services().backend.addHostedEffectFromFile(juce::File(path),
                                                                            safe->blockId());
            if (id.empty())
              safe->services().toast.show("Could not load this effect");
            else
              safe->services().chain.refresh(true);
          });
    }

    juce::PopupMenu formatMenu;
    for (auto& letterPair : byLetter)
      formatMenu.addSubMenu(letterPair.first, letterPair.second);
    menu.addSubMenu(formatPair.first + " (" + juce::String(static_cast<int>(items.size())) + ")",
                    formatMenu);
  }
  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this));
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
