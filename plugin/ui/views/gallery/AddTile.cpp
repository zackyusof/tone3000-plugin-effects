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
    juce::String format;
    juce::String path;
  };
  // Group by vendor (from each bundle's reverse-DNS id). With 1000+ installed
  // effects this keeps the menu navigable without a blocking scan.
  std::map<juce::String, std::vector<Entry>> byVendor;
  for (const auto& effect : *array) {
    Entry entry{effect["name"].toString(),
                effect["format"].toString() == "VST3" ? juce::String("VST3") : juce::String("AU"),
                effect["path"].toString()};
    byVendor[effect["vendor"].toString()].push_back(std::move(entry));
  }

  juce::PopupMenu menu;
  for (auto& vendorPair : byVendor) {
    auto& items = vendorPair.second;
    std::sort(items.begin(), items.end(), [](const Entry& a, const Entry& b) {
      return a.name.compareIgnoreCase(b.name) < 0;
    });

    juce::PopupMenu vendorMenu;
    for (size_t i = 0; i < items.size(); ++i) {
      juce::String label = items[i].name;
      // Same name twice under one vendor (e.g. AU + VST3): disambiguate by format.
      for (size_t j = 0; j < items.size(); ++j) {
        if (j != i && items[j].name == items[i].name) {
          label << " (" << items[i].format << ")";
          break;
        }
      }
      vendorMenu.addItem(
          label, true, false,
          [safe = juce::Component::SafePointer<AddTile>(this), path = items[i].path] {
            if (safe == nullptr) return;
            const auto id = safe->services().backend.addHostedEffectFromFile(juce::File(path),
                                                                            safe->blockId());
            if (id.empty())
              safe->services().toast.show("Could not load this effect");
            else
              safe->services().chain.refresh(true);
          });
    }
    menu.addSubMenu(vendorPair.first + " (" + juce::String(static_cast<int>(items.size())) + ")",
                    vendorMenu);
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
