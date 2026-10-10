#include "Processor.h"

#include <algorithm>

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

}  // namespace

juce::var TONE3000Processor::listHostedEffects() const {
  juce::Array<juce::var> result;
  auto formats = makeFormatManager();
  for (int i = 0; i < formats->getNumFormats(); ++i) {
    auto* format = formats->getFormat(i);
    if (format == nullptr || (format->getName() != "AudioUnit" && format->getName() != "VST3"))
      continue;
    auto identifiers = format->searchPathsForPlugins(format->getDefaultLocationsToSearch(),
                                                       true, false);
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
