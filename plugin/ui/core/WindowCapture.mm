#include "WindowCapture.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

namespace t3k::ui {

juce::Image captureComponentWindow(juce::Component& component) {
  auto* peer = component.getPeer();
  if (peer == nullptr)
    return {};
  NSView* view = (NSView*) peer->getNativeHandle();
  if (view == nil)
    return {};
  NSWindow* window = [view window];
  if (window == nil)
    return {};
  const CGWindowID number = (CGWindowID) [window windowNumber];
  // kCGWindowListOptionIncludingWindow captures this app's own window (real
  // composited pixels, including native/Metal views).
  CGImageRef cg = CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow,
                                          number, kCGWindowImageBoundsIgnoreFraming);
  if (cg == nullptr)
    return {};
  NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithCGImage:cg];
  CGImageRelease(cg);
  if (rep == nil)
    return {};
  NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
  if (png == nil)
    return {};
  return juce::ImageFileFormat::loadFrom([png bytes], static_cast<size_t>([png length]));
}

}  // namespace t3k::ui
