#include "HostedEffectWindow.h"

#include "backend/Backend.h"

namespace t3k::ui {

namespace {
// A top-level window that hosts the plugin's own editor (or a generic panel
// when the plugin ships no custom UI). Closing it deletes the window; the
// plugin instance is shared-owned, so the effect stays in the chain.
class HostedEffectWindow : public juce::DocumentWindow {
public:
  HostedEffectWindow(std::shared_ptr<juce::AudioPluginInstance> instance,
                     const juce::String& name)
      : juce::DocumentWindow(name, juce::Colours::darkgrey,
                             juce::DocumentWindow::closeButton),
        instance_(std::move(instance)) {
    auto* editor = instance_->createEditorAndMakeActive();
    if (editor == nullptr)
      editor = new juce::GenericAudioProcessorEditor(*instance_);
    setContentOwned(editor, true);
    setUsingNativeTitleBar(true);
    centreWithSize(juce::jmax(400, editor->getWidth()),
                   juce::jmax(300, editor->getHeight()));
    setVisible(true);
    toFront(true);
  }
  ~HostedEffectWindow() override { clearContentComponent(); }
  void closeButtonPressed() override { delete this; }

private:
  std::shared_ptr<juce::AudioPluginInstance> instance_;
};
}  // namespace

bool openHostedEffectEditor(Services& services, const std::string& blockId,
                            const juce::String& name) {
  auto instance = services.backend.hostedEffectInstance(blockId);
  if (instance == nullptr)
    return false;
  new HostedEffectWindow(std::move(instance), name);
  return true;
}

}  // namespace t3k::ui
