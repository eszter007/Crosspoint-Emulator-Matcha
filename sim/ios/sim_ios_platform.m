// iOS platform glue for the emulator.
//
// SDL2 has no safe-area API, so the insets the layout needs -- the sensor
// housing at the top and the home indicator at the bottom -- are read straight
// from UIKit. Everything else the emulator needs from the platform is already
// covered by SDL.

#import <UIKit/UIKit.h>

void sim_platform_safe_area(float* top, float* bottom, float* left, float* right) {
  if (top) *top = 0.0f;
  if (bottom) *bottom = 0.0f;
  if (left) *left = 0.0f;
  if (right) *right = 0.0f;

  // Called from the render/main loop; UIKit is main-thread only. The emulator
  // only calls this from its main thread (layout()), but bail rather than risk
  // it if that ever changes.
  if (![NSThread isMainThread]) return;

  UIWindow* window = nil;
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    if (![scene isKindOfClass:UIWindowScene.class]) continue;
    for (UIWindow* candidate in ((UIWindowScene*)scene).windows) {
      if (candidate.isKeyWindow) {
        window = candidate;
        break;
      }
    }
    if (window) break;
  }
  if (!window) window = UIApplication.sharedApplication.windows.firstObject;
  if (!window) return;

  const UIEdgeInsets insets = window.safeAreaInsets;
  if (top) *top = (float)insets.top;
  if (bottom) *bottom = (float)insets.bottom;
  if (left) *left = (float)insets.left;
  if (right) *right = (float)insets.right;
}

// The SD card is the app's Documents directory: the only writable location iOS
// gives it, and -- with UIFileSharingEnabled and
// LSSupportsOpeningDocumentsInPlace in Info.plist -- the one the Files app
// shows. Dropping an EPUB in there is how a book gets "onto the card".
const char* sim_platform_sdcard_root(void) {
  static char cached[1024];
  if (cached[0]) return cached;
  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
  NSString* documents = paths.firstObject;
  if (!documents) return NULL;
  strlcpy(cached, documents.UTF8String, sizeof(cached));
  return cached;
}
