#include "Processor.h"

#include <algorithm>
#include <map>

#if JUCE_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace {

std::unique_ptr<juce::AudioPluginFormatManager> makeFormatManager() {
  auto formats = std::make_unique<juce::AudioPluginFormatManager>();
  juce::addDefaultFormatsToManager(*formats);
  return formats;
}

bool parseDescription(const juce::String& xml, juce::PluginDescription& description) {
  auto document = juce::XmlDocument::parse(xml);
  return document != nullptr && description.loadFromXml(*document) &&
         !description.isInstrument;
}

std::shared_ptr<juce::AudioPluginInstance> instantiateEffect(
    const juce::String& xml, const juce::MemoryBlock& state, double rate, int blockSize) {
  juce::PluginDescription description;
  if (!parseDescription(xml, description))
    return {};

  auto formats = makeFormatManager();
  juce::String error;
  auto instance = formats->createPluginInstance(description, rate, blockSize, error);
  if (instance == nullptr) {
    juce::Logger::writeToLog("[Hosted effect] " + description.name + ": " + error);
    return {};
  }
  if (instance->getTotalNumInputChannels() < 1 ||
      instance->getTotalNumInputChannels() > 2 ||
      instance->getTotalNumOutputChannels() < 1 ||
      instance->getTotalNumOutputChannels() > 2) {
    juce::Logger::writeToLog("[Hosted effect] Unsupported channel layout: " + description.name);
    return {};
  }
  instance->setRateAndBufferSizeDetails(rate, blockSize);
  instance->prepareToPlay(rate, blockSize);
  if (!state.isEmpty())
    instance->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
  return std::shared_ptr<juce::AudioPluginInstance>(std::move(instance));
}

#if JUCE_MAC
// Reads the bundle's CFBundleIdentifier without loading/instantiating the plugin
// (CFBundleCreate just parses Info.plist — cheap and safe on the message thread).
juce::String bundleIdentifier(const juce::File& bundle) {
  const auto path = bundle.getFullPathName();
  CFURLRef url = CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(path.toRawUTF8()),
      static_cast<CFIndex>(path.getNumBytesAsUTF8()), true);
  if (url == nullptr)
    return {};
  CFBundleRef cfBundle = CFBundleCreate(kCFAllocatorDefault, url);
  CFRelease(url);
  if (cfBundle == nullptr)
    return {};
  juce::String result;
  if (CFStringRef id = CFBundleGetIdentifier(cfBundle)) {
    const CFIndex len = CFStringGetLength(id);
    const CFRange range = CFRangeMake(0, len);
    CFIndex used = 0;
    CFStringGetBytes(id, range, kCFStringEncodingUTF8, 0, false, nullptr, 0, &used);
    juce::HeapBlock<UInt8> buffer(static_cast<size_t>(used) + 1);
    CFStringGetBytes(id, range, kCFStringEncodingUTF8, 0, false,
                     reinterpret_cast<UInt8*>(buffer.getData()), used, nullptr);
    buffer[static_cast<size_t>(used)] = 0;
    result = juce::String::fromUTF8(reinterpret_cast<const char*>(buffer.getData()));
  }
  CFRelease(cfBundle);
  return result;
}
#endif

// Vendor label from a reverse-DNS bundle id, e.g. "com.fabfilter.Pro-Q.4" -> "FabFilter".
juce::String vendorFromBundleId(const juce::String& id) {
  if (id.isEmpty())
    return "Other";
  auto parts = juce::StringArray::fromTokens(id, ".", "");
  parts.trim();
  if (parts.isEmpty())
    return "Other";
  static const juce::StringArray kTlds{
      "com", "org", "net", "io", "co", "audio", "de", "se", "uk", "fr", "jp",
      "eu", "us", "me", "it", "nl", "ca", "ch", "at", "au", "tv", "app", "dev", "info", "biz"};
  int i = 0;
  while (i < parts.size() && kTlds.contains(parts[i].toLowerCase()))
    ++i;
  juce::String vendor = (i < parts.size()) ? parts[i] : parts.getReference(0);
  if (vendor.isEmpty())
    return "Other";
  // Normalise the grouping key (case + separators) so e.g. "plugin-alliance",
  // "pluginalliance" and "Pluginalliance" collapse to one vendor.
  const auto key = vendor.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789");
  // Display names for vendors whose reverse-DNS label is not their name.
  static const std::map<juce::String, juce::String> kAliases{
      {"uaudio", "Universal Audio"}, {"slatedigital", "Slate Digital"},
      {"pluginalliance", "Plugin Alliance"}, {"izotope", "iZotope"},
      {"fabfilter", "FabFilter"}, {"toneempire", "Tone Empire"},
      {"wavesaudio", "Waves"}, {"soundtheory", "Soundtheory"},
      {"bertomaudio", "Bertom Audio"}, {"ikmultimedia", "IK Multimedia"},
      {"nativeinstruments", "Native Instruments"}, {"neuraldsp", "Neural DSP"},
      {"blackroosteraudio", "Black Rooster Audio"}, {"unitedplugins", "United Plugins"},
      {"inmusicbrands", "inMusic Brands"}, {"theusualsuspects", "The Usual Suspects"},
      {"mcdsp", "McDSP"}, {"newfangledaudio", "Newfangled Audio"},
      {"lindellplugins", "Lindell Plugins"}, {"kiiveaudio", "Kiive Audio"},
      {"babyaudio", "BABY Audio"}, {"soundtoys", "Soundtoys"},
      {"softube", "Softube"}, {"eventide", "Eventide"}, {"ssl", "SSL"}};
  const auto alias = kAliases.find(key);
  if (alias != kAliases.end())
    return alias->second;
  return vendor.substring(0, 1).toUpperCase() + vendor.substring(1);
}

}  // namespace

juce::String TONE3000Processor::vendorForBundle(const juce::String& bundlePath) {
#if JUCE_MAC
  return vendorFromBundleId(bundleIdentifier(juce::File(bundlePath)));
#else
  juce::ignoreUnused(bundlePath);
  return "Other";
#endif
}

juce::var TONE3000Processor::listHostedEffectBundles() const {
  juce::Array<juce::var> result;
  auto addDir = [&result](const juce::String& dir, const juce::String& wildcard, const juce::String& format) {
    const juce::File folder(dir);
    if (!folder.isDirectory())
      return;
    for (const auto& entry : juce::RangedDirectoryIterator(
             folder, /*recursive*/ false, wildcard, juce::File::findDirectories)) {
      auto* item = new juce::DynamicObject();
      item->setProperty("name", entry.getFile().getFileNameWithoutExtension());
      item->setProperty("format", format);
      item->setProperty("path", entry.getFile().getFullPathName());
#if JUCE_MAC
      item->setProperty("vendor", vendorFromBundleId(bundleIdentifier(entry.getFile())));
#else
      item->setProperty("vendor", "Other");
#endif
      result.add(juce::var(item));
    }
  };
  addDir("/Library/Audio/Plug-Ins/Components", "*.component", "AudioUnit");
  addDir("/Library/Audio/Plug-Ins/VST3", "*.vst3", "VST3");
  return result;
}

juce::var TONE3000Processor::listHostedEffects() const {
  juce::Array<juce::var> result;
  auto formats = makeFormatManager();
  for (int i = 0; i < formats->getNumFormats(); ++i) {
    auto* format = formats->getFormat(i);
    if (format == nullptr || (format->getName() != "AudioUnit" && format->getName() != "VST3"))
      continue;
    // Hardcoded standard macOS locations, one per format, instead of every default
    // location JUCE would otherwise walk. Predictable and fast.
    juce::FileSearchPath searchPaths;
    if (format->getName() == "AudioUnit")
      searchPaths.add(juce::File("/Library/Audio/Plug-Ins/Components"));
    else if (format->getName() == "VST3")
      searchPaths.add(juce::File("/Library/Audio/Plug-Ins/VST3"));
    auto identifiers = format->searchPathsForPlugins(searchPaths, false, false);
    for (const auto& identifier : identifiers) {
      juce::OwnedArray<juce::PluginDescription> descriptions;
      format->findAllTypesForFile(descriptions, identifier);
      for (const auto* description : descriptions) {
        if (description == nullptr || description->isInstrument ||
            description->name.containsIgnoreCase("TONE3000"))
          continue;
        auto xml = description->createXml();
        if (xml == nullptr)
          continue;
        auto* item = new juce::DynamicObject();
        item->setProperty("name", description->name);
        item->setProperty("manufacturer", description->manufacturerName);
        item->setProperty("format", format->getName());
        item->setProperty("description", xml->toString());
        result.add(juce::var(item));
      }
    }
  }
  return result;
}

std::string TONE3000Processor::addHostedEffect(const juce::String& xml,
                                                const std::string& targetInsertId) {
  juce::PluginDescription description;
  if (!parseDescription(xml, description))
    return {};
  auto instance = instantiateEffect(xml, {}, chainSampleRate(), chainDomainBlockSize());
  if (instance == nullptr)
    return {};

  ChainEditFade editFade(*this);
  juce::ScopedLock lock(chainMutex);
  pushChainHistory();
  const std::string id = juce::Uuid().toString().toStdString();
  auto block = std::make_unique<ChainBlock>(id, ChainBlockType::EFFECT);
  block->effectDescriptionXml = xml;
  block->effectName = description.name;
  block->effectInstance = std::move(instance);
  block->loaded = true;
  block->effectScratch.setSize(juce::jmax(block->effectInstance->getTotalNumInputChannels(),
                                         block->effectInstance->getTotalNumOutputChannels()),
                               chainDomainBlockSize());
  prepareBlockForChainRate(*block);

  Lane* target = nullptr;
  Lane::iterator slot;
  for (auto& laneBlocks : lanes) {
    auto it = std::find_if(laneBlocks.begin(), laneBlocks.end(), [&](const auto& candidate) {
      return candidate->type == ChainBlockType::INSERT && candidate->id == targetInsertId;
    });
    if (it != laneBlocks.end()) {
      target = &laneBlocks;
      slot = it;
      break;
    }
  }
  if (target == nullptr) {
    target = &activeChain();
    slot = std::find_if(target->begin(), target->end(), [](const auto& candidate) {
      return candidate->type == ChainBlockType::INSERT;
    });
  }
  if (slot != target->end())
    *slot = std::move(block);
  else
    target->push_back(std::move(block));
  alignBranchLaneLengths();
  bumpChainRevision();
  triggerAsyncUpdate();
  return id;
}

std::string TONE3000Processor::addHostedEffectFromFile(
    const juce::File& bundle, const std::string& targetInsertId) {
  if (!bundle.hasFileExtension(".component") && !bundle.hasFileExtension(".vst3"))
    return {};
  auto formats = makeFormatManager();
  for (int i = 0; i < formats->getNumFormats(); ++i) {
    auto* format = formats->getFormat(i);
    if (format == nullptr ||
        (bundle.hasFileExtension(".component") && format->getName() != "AudioUnit") ||
        (bundle.hasFileExtension(".vst3") && format->getName() != "VST3"))
      continue;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format->findAllTypesForFile(descriptions, bundle.getFullPathName());
    for (const auto* description : descriptions) {
      if (description == nullptr || description->isInstrument)
        continue;
      if (auto xml = description->createXml())
        return addHostedEffect(xml->toString(), targetInsertId);
    }
  }
  return {};
}

void TONE3000Processor::restoreHostedEffect(ChainBlock& block) {
  block.effectInstance = instantiateEffect(block.effectDescriptionXml, block.effectState,
                                            chainSampleRate(), chainDomainBlockSize());
  block.loaded = block.effectInstance != nullptr;
  if (block.loaded)
    block.effectScratch.setSize(juce::jmax(block.effectInstance->getTotalNumInputChannels(),
                                          block.effectInstance->getTotalNumOutputChannels()),
                                chainDomainBlockSize());
  block.loadFailed = !block.loaded;
  block.modelLoading = false;
}

std::shared_ptr<juce::AudioPluginInstance> TONE3000Processor::hostedEffectInstance(
    const std::string& blockId) {
  juce::ScopedLock lock(chainMutex);
  auto* block = findBlockById(blockId);
  return block != nullptr && block->type == ChainBlockType::EFFECT
             ? block->effectInstance : std::shared_ptr<juce::AudioPluginInstance>{};
}
