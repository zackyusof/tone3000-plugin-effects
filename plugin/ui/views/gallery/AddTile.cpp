#include "AddTile.h"

#include "GalleryGeometry.h"
#include "core/Help.h"
#include "core/Icons.h"
#include "core/Paint.h"
#include "core/Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace t3k::ui {

namespace {
// Mirrors Element's plugin menu builder (kushview/element, ui/contextmenus.cpp):
// walk a KnownPluginList tree and emit one submenu per folder, one item per plugin.
// Element builds the tree with KnownPluginList::createTree(types, sortByManufacturer).
void addPluginTreeItems(juce::PopupMenu& menu,
                        const juce::KnownPluginList::PluginTree& tree,
                        const std::function<void(const juce::PluginDescription&)>& onChosen) {
  for (auto* sub : tree.subFolders) {
    juce::PopupMenu subMenu;
    addPluginTreeItems(subMenu, *sub, onChosen);
    menu.addSubMenu(sub->folder, subMenu);
  }
  for (const auto& desc : tree.plugins) {
    juce::String name = desc.name;
    if (desc.pluginFormatName.isNotEmpty())
      name << " (" << desc.pluginFormatName << ")";
    menu.addItem(name, true, false, [onChosen, desc] { onChosen(desc); });
  }
}
}  // namespace

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
  const auto listed = services().backend.listHostedEffects();
  const auto* array = listed.getArray();
  if (array == nullptr || array->isEmpty()) {
    services().toast.show("No AU/VST3 effects found");
    return;
  }

  // Build a KnownPluginList from the scanned descriptions, then render it as
  // nested submenus grouped by manufacturer (Element's pattern) instead of a
  // flat list or a Finder dialog.
  juce::KnownPluginList known;
  for (const auto& effect : *array) {
    if (auto xml = juce::XmlDocument::parse(effect["description"].toString())) {
      juce::PluginDescription desc;
      if (desc.loadFromXml(*xml) && !desc.isInstrument)
        known.addType(desc);
    }
  }
  auto tree = juce::KnownPluginList::createTree(known.getTypes(),
                                                juce::KnownPluginList::sortByManufacturer);
  if (tree == nullptr || (tree->plugins.isEmpty() && tree->subFolders.isEmpty())) {
    services().toast.show("No AU/VST3 effects found");
    return;
  }

  juce::PopupMenu menu;
  addPluginTreeItems(menu, *tree,
                     [safe = juce::Component::SafePointer<AddTile>(this)](
                         const juce::PluginDescription& desc) {
                       if (safe == nullptr) return;
                       auto xml = desc.createXml();
                       if (xml == nullptr) return;
                       const auto id = safe->services().backend.addHostedEffect(
                           xml->toString(), safe->blockId());
                       if (id.empty())
                         safe->services().toast.show("Could not load this effect");
                       else
                         safe->services().chain.refresh(true);
                     });
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
