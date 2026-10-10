#include "HostedEffectWindow.h"

#include <map>

#include "backend/Backend.h"
#include "core/WindowCapture.h"

namespace t3k::ui {

namespace {

juce::ChangeBroadcaster& graphicBroadcaster() {
  static juce::ChangeBroadcaster broadcaster;
  return broadcaster;
}

juce::CriticalSection& graphicLock() {
  static juce::CriticalSection lock;
  return lock;
}

std::map<std::string, juce::Image>& graphicMap() {
  static std::map<std::string, juce::Image> images;
  return images;
}

// A top-level window hosting the plugin's own editor. Closing it deletes the
// window; the plugin instance is shared-owned, so the effect stays in the chain.
// After it has painted once it snapshots the editor as the slot's static graphic.
class HostedEffectWindow : public juce::DocumentWindow, private juce::Timer {
public:
  HostedEffectWindow(std::shared_ptr<juce::AudioPluginInstance> instance,
                     const juce::String& name, std::string blockId)
      : juce::DocumentWindow(name, juce::Colours::darkgrey,
                             juce::DocumentWindow::closeButton),
        instance_(std::move(instance)),
        blockId_(std::move(blockId)) {
    auto* editor = instance_->createEditorAndMakeActive();
    if (editor == nullptr)
      editor = new juce::GenericAudioProcessorEditor(*instance_);
    setContentOwned(editor, true);
    setUsingNativeTitleBar(true);
    centreWithSize(juce::jmax(400, editor->getWidth()),
                   juce::jmax(300, editor->getHeight()));
    setVisible(true);
    toFront(true);
    startTimer(400);  // capture after it has painted on screen
  }
  ~HostedEffectWindow() override {
    stopTimer();
    clearContentComponent();
  }
  void closeButtonPressed() override { delete this; }

private:
  void timerCallback() override {
    stopTimer();
    // Real on-screen pixels: a JUCE component snapshot is blank for native views.
    auto image = captureComponentWindow(*this);
    if (!image.isValid()) return;
    {
      const juce::ScopedLock sl(graphicLock());
      graphicMap()[blockId_] = image;
    }
    graphicBroadcaster().sendChangeMessage();
  }

  std::shared_ptr<juce::AudioPluginInstance> instance_;
  std::string blockId_;
};

}  // namespace

juce::Image hostedEffectGraphic(const std::string& blockId) {
  const juce::ScopedLock sl(graphicLock());
  auto it = graphicMap().find(blockId);
  return it != graphicMap().end() ? it->second : juce::Image();
}

juce::ChangeBroadcaster& hostedEffectGraphics() { return graphicBroadcaster(); }

bool openHostedEffectEditor(Services& services, const std::string& blockId,
                            const juce::String& name) {
  auto instance = services.backend.hostedEffectInstance(blockId);
  if (instance == nullptr)
    return false;
  new HostedEffectWindow(std::move(instance), name, blockId);
  return true;
}

}  // namespace t3k::ui
