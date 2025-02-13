// Copyright (c) [2013-2015] WhiteHat. All Rights Reserved. WhiteHat contributions included 
// in this file are licensed under the BSD-3-clause license (the "License") included in 
// the WhiteHat-LICENSE file included in the root directory of the distributed source code 
// archive. Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF
// ANY KIND, either express or implied. See the License for the specific language governing 
// permissions and limitations under the License. 


#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_H_

// A class acting as the Objective-C controller for the Browser
// object. Handles interactions between Cocoa and the cross-platform
// code. Each window has a single toolbar and, by virtue of being a
// TabWindowController, a tab strip along the top.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser_command_executor.h"
#import "chrome/browser/ui/cocoa/fullscreen_exit_bubble_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_window_controller.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/url_drop_target.h"
#import "chrome/browser/ui/cocoa/view_resizer.h"
#include "components/translate/core/common/translate_errors.h"
#include "ui/gfx/rect.h"
#import "chrome/browser/championconfig/ProtectedUnprotectedViewMac/unprotected_bubble_controller.h" // champion_unprotected
#import "chrome/browser/championconfig/ProtectedUnprotectedViewMac/protected_bubble_controller.h" // champion_protected added
extern bool isShowUnprotectedBubble;
extern bool isShowProtectedBubble; // champion_protected added

@class AvatarBaseController;
class Browser;
class BrowserWindow;
class BrowserWindowCocoa;
@class DevToolsController;
@class DownloadShelfController;
class ExtensionKeybindingRegistryCocoa;
@class FindBarCocoaController;
@class FullscreenModeController;
@class FullscreenWindow;
@class InfoBarContainerController;
class LocationBarViewMac;
@class OverlayableContentsController;
class PermissionBubbleCocoa;
@class PresentationModeController;
class StatusBubbleMac;
@class TabStripController;
@class TabStripView;
@class ToolbarController;
@class TranslateBubbleController;

namespace content {
class WebContents;
}

namespace extensions {
class Command;
}

@interface BrowserWindowController :
  TabWindowController<NSUserInterfaceValidations,
                      BookmarkBarControllerDelegate,
                      BrowserCommandExecutor,
                      ViewResizer,
                      TabStripControllerDelegate> {
 @private
  // The ordering of these members is important as it determines the order in
  // which they are destroyed. |browser_| needs to be destroyed last as most of
  // the other objects hold weak references to it or things it owns
  // (tab/toolbar/bookmark models, profiles, etc).
  NSTimer *timerUnprotected; // champion_unprotected
  NSTimer *timerProtected; // champion_protected added
  scoped_ptr<Browser> browser_;
  NSWindow* savedRegularWindow_;
  scoped_ptr<BrowserWindowCocoa> windowShim_;
  base::scoped_nsobject<ToolbarController> toolbarController_;
  base::scoped_nsobject<TabStripController> tabStripController_;
  base::scoped_nsobject<FindBarCocoaController> findBarCocoaController_;
  base::scoped_nsobject<InfoBarContainerController> infoBarContainerController_;
  base::scoped_nsobject<DownloadShelfController> downloadShelfController_;
  base::scoped_nsobject<BookmarkBarController> bookmarkBarController_;
  base::scoped_nsobject<DevToolsController> devToolsController_;
  base::scoped_nsobject<OverlayableContentsController>
      overlayableContentsController_;
  base::scoped_nsobject<PresentationModeController> presentationModeController_;
  base::scoped_nsobject<FullscreenModeController> fullscreenModeController_;
  base::scoped_nsobject<FullscreenExitBubbleController>
      fullscreenExitBubbleController_;

  // Strong. StatusBubble is a special case of a strong reference that
  // we don't wrap in a scoped_ptr because it is acting the same
  // as an NSWindowController in that it wraps a window that must
  // be shut down before our destructors are called.
  StatusBubbleMac* statusBubble_;

  BookmarkBubbleController* bookmarkBubbleController_;  // Weak.
  UnprotectedBubbleController* unprotectedBubbleController_;  // Weak. // champion_unprotected
  ProtectedBubbleController* protectedBubbleController_;  // Weak. // champion_protected added
  BOOL initializing_;  // YES while we are currently in initWithBrowser:
  BOOL ownsBrowser_;  // Only ever NO when testing

  TranslateBubbleController* translateBubbleController_;  // Weak.

  // The total amount by which we've grown the window up or down (to display a
  // bookmark bar and/or download shelf), respectively; reset to 0 when moved
  // away from the bottom/top or resized (or zoomed).
  CGFloat windowTopGrowth_;
  CGFloat windowBottomGrowth_;

  // YES only if we're shrinking the window from an apparent zoomed state (which
  // we'll only do if we grew it to the zoomed state); needed since we'll then
  // restrict the amount of shrinking by the amounts specified above. Reset to
  // NO on growth.
  BOOL isShrinkingFromZoomed_;

  // The view controller that manages the incognito badge or the multi-profile
  // avatar button. Depending on whether the --new-profile-management flag is
  // used, the multi-profile button can either be the avatar's icon badge or a
  // button with the profile's name. If the flag is used, the button is always
  // shown, otherwise the view will always be in the view hierarchy but will
  // be hidden unless it's appropriate to show it (i.e. if there's more than
  // one profile).
  base::scoped_nsobject<AvatarBaseController> avatarButtonController_;

  // Lazily created view which draws the background for the floating set of bars
  // in presentation mode (for window types having a floating bar; it remains
  // nil for those which don't).
  base::scoped_nsobject<NSView> floatingBarBackingView_;

  // The borderless window used in fullscreen mode when Cocoa's System
  // Fullscreen API is not being used (or not available, before OS 10.7).
  base::scoped_nsobject<NSWindow> fullscreenWindow_;

  // The Cocoa implementation of the PermissionBubbleView.
  scoped_ptr<PermissionBubbleCocoa> permissionBubbleCocoa_;

  // True between |-windowWillEnterFullScreen:| and |-windowDidEnterFullScreen:|
  // to indicate that the window is in the process of transitioning into
  // fullscreen mode.
  BOOL enteringFullscreen_;

  // True between |-setPresentationMode:url:bubbleType:| and
  // |-windowDidEnterFullScreen:| to indicate that the window is in the process
  // of transitioning into fullscreen presentation mode.
  BOOL enteringPresentationMode_;

  // The size of the original (non-fullscreen) window.  This is saved just
  // before entering fullscreen mode and is only valid when |-isFullscreen|
  // returns YES.
  NSRect savedRegularWindowFrame_;

  // The proportion of the floating bar which is shown (in presentation mode).
  CGFloat floatingBarShownFraction_;

  // Various UI elements/events may want to ensure that the floating bar is
  // visible (in presentation mode), e.g., because of where the mouse is or
  // where keyboard focus is. Whenever an object requires bar visibility, it has
  // itself added to |barVisibilityLocks_|. When it no longer requires bar
  // visibility, it has itself removed.
  base::scoped_nsobject<NSMutableSet> barVisibilityLocks_;

  // Bar visibility locks and releases only result (when appropriate) in changes
  // in visible state when the following is |YES|.
  BOOL barVisibilityUpdatesEnabled_;

  // When going fullscreen for a tab, we need to store the URL and the
  // fullscreen type, since we can't show the bubble until
  // -windowDidEnterFullScreen: gets called.
  GURL fullscreenUrl_;
  FullscreenExitBubbleType fullscreenBubbleType_;

  // The Extension Command Registry used to determine which keyboard events to
  // handle.
  scoped_ptr<ExtensionKeybindingRegistryCocoa> extension_keybinding_registry_;

  // The number of overlapped views being shown.
  NSUInteger overlappedViewCount_;
}

// A convenience class method which gets the |BrowserWindowController| for a
// given window. This method returns nil if no window in the chain has a BWC.
+ (BrowserWindowController*)browserWindowControllerForWindow:(NSWindow*)window;

// A convenience class method which gets the |BrowserWindowController| for a
// given view.  This is the controller for the window containing |view|, if it
// is a BWC, or the first controller in the parent-window chain that is a
// BWC. This method returns nil if no window in the chain has a BWC.
+ (BrowserWindowController*)browserWindowControllerForView:(NSView*)view;

// Helper method used to update the "Signin" menu item to reflect the current
// signed in state. Class-level function as it's still required even when there
// are no open browser windows.
+ (void)updateSigninItem:(id)signinItem
              shouldShow:(BOOL)showSigninMenuItem
          currentProfile:(Profile*)profile;

// Load the browser window nib and do any Cocoa-specific initialization.
// Takes ownership of |browser|.
- (id)initWithBrowser:(Browser*)browser;

// Call to make the browser go away from other places in the cross-platform
// code.
- (void)destroyBrowser;

// Ensure bounds for the window abide by the minimum window size.
- (gfx::Rect)enforceMinWindowSize:(gfx::Rect)bounds;

// Access the C++ bridge between the NSWindow and the rest of Chromium.
- (BrowserWindow*)browserWindow;

// Return a weak pointer to the toolbar controller.
- (ToolbarController*)toolbarController;

// Return a weak pointer to the tab strip controller.
- (TabStripController*)tabStripController;

// Return a weak pointer to the find bar controller.
- (FindBarCocoaController*)findBarCocoaController;

// Access the ObjC controller that contains the infobars.
- (InfoBarContainerController*)infoBarContainerController;

// Access the C++ bridge object representing the status bubble for the window.
- (StatusBubbleMac*)statusBubble;

// Access the C++ bridge object representing the location bar.
- (LocationBarViewMac*)locationBarBridge;

// Returns a weak pointer to the floating bar backing view;
- (NSView*)floatingBarBackingView;

// Returns a weak pointer to the overlayable contents controller.
- (OverlayableContentsController*)overlayableContentsController;

// Access the Profile object that backs this Browser.
- (Profile*)profile;

// Access the avatar button controller.
- (AvatarBaseController*)avatarButtonController;

// Forces the toolbar (and transitively the location bar) to update its current
// state.  If |tab| is non-NULL, we're switching (back?) to this tab and should
// restore any previous location bar state (such as user editing) as well.
- (void)updateToolbarWithContents:(content::WebContents*)tab;

// Sets whether or not the current page in the frontmost tab is bookmarked.
- (void)setStarredState:(BOOL)isStarred;

// Sets whether or not the current page is translated.
- (void)setCurrentPageIsTranslated:(BOOL)on;

// Happens when the zoom level is changed in the active tab, the active tab is
// changed, or a new browser window or tab is created. |canShowBubble| denotes
// whether it would be appropriate to show a zoom bubble or not.
- (void)zoomChangedForActiveTab:(BOOL)canShowBubble;

// Return the rect, in WebKit coordinates (flipped), of the window's grow box
// in the coordinate system of the content area of the currently selected tab.
- (NSRect)selectedTabGrowBoxRect;

// Called to tell the selected tab to update its loading state.
// |force| is set if the update is due to changing tabs, as opposed to
// the page-load finishing.  See comment in reload_button.h.
- (void)setIsLoading:(BOOL)isLoading force:(BOOL)force;

// Brings this controller's window to the front.
- (void)activate;

// Make the location bar the first responder, if possible.
- (void)focusLocationBar:(BOOL)selectAll;

// Make the (currently-selected) tab contents the first responder, if possible.
- (void)focusTabContents;

// Returns the frame of the regular (non-fullscreened) window (even if the
// window is currently in fullscreen mode).  The frame is returned in Cocoa
// coordinates (origin in bottom-left).
- (NSRect)regularWindowFrame;

// Whether or not to show the avatar, which is either the incognito guy or the
// user's profile avatar.
- (BOOL)shouldShowAvatar;

// Whether or not to show the new avatar button used by --new-profile-maagement.
- (BOOL)shouldUseNewAvatarButton;

- (BOOL)isBookmarkBarVisible;

// Returns YES if the bookmark bar is currently animating.
- (BOOL)isBookmarkBarAnimating;

- (BookmarkBarController*)bookmarkBarController;

- (DevToolsController*)devToolsController;

- (BOOL)isDownloadShelfVisible;

// Lazily creates the download shelf in visible state if it doesn't exist yet.
- (DownloadShelfController*)downloadShelf;

// Retains the given FindBarCocoaController and adds its view to this
// browser window.  Must only be called once per
// BrowserWindowController.
- (void)addFindBar:(FindBarCocoaController*)findBarCocoaController;

// The user changed the theme.
- (void)userChangedTheme;

// Executes the command in the context of the current browser.
// |command| is an integer value containing one of the constants defined in the
// "chrome/app/chrome_command_ids.h" file.
- (void)executeCommand:(int)command;

// Consults the Command Registry to see if this |event| needs to be handled as
// an extension command and returns YES if so (NO otherwise).
- (BOOL)handledByExtensionCommand:(NSEvent*)event;

// Delegate method for the status bubble to query its base frame.
- (NSRect)statusBubbleBaseFrame;

// Show the bookmark bubble (e.g. user just clicked on the STAR)
- (void)showBookmarkBubbleForURL:(const GURL&)url
               alreadyBookmarked:(BOOL)alreadyBookmarked;
- (void)showUnprotectedBubble; // champion_unprotected
- (void)showProtectedBubble; // champion_protected added
- (void)closeUnprotectedBubble; // champion_unprotected
- (void)closeProtectedBubble; // champion_protected added

// Show the translate bubble.
- (void)showTranslateBubbleForWebContents:(content::WebContents*)contents
                                     step:(translate::TranslateStep)step
                                errorType:(TranslateErrors::Type)errorType;

// Shows or hides the docked web inspector depending on |contents|'s state.
- (void)updateDevToolsForContents:(content::WebContents*)contents;

// Gets the current theme provider.
- (ui::ThemeProvider*)themeProvider;

// Gets the window style.
- (ThemedWindowStyle)themedWindowStyle;

// Returns the position in the coordinates of the root view
// ([[self contentView] superview]) that the top left of a theme image with
// |alignment| should be painted at. If the window does not have a tab strip,
// the offset for THEME_IMAGE_ALIGN_WITH_FRAME is always returned. The result of
// this method can be used in conjunction with
// [NSGraphicsContext cr_setPatternPhase:] to set the offset of pattern colors.
- (NSPoint)themeImagePositionForAlignment:(ThemeImageAlignment)alignment;

// Return the point to which a bubble window's arrow should point, in window
// coordinates.
- (NSPoint)bookmarkBubblePoint;

- (NSPoint)unprotectedBubblePoint; // champion_unprotected
- (NSPoint)protectedBubblePoint; // champion_protected added
// Called when the Add Search Engine dialog is closed.
- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(NSInteger)code
            context:(void*)context;

// Called when the find bar visibility changes. This is used to update the
// allowOverlappingViews state.
- (void)onFindBarVisibilityChanged;

// Called when an overlapped view is shown. This is used to update the
// allowOverlappingViews state. Currently used for history overlay and
// confirm bubble.
- (void)onOverlappedViewShown;

// Called when a history overlay is hidden. This is used to update the
// allowOverlappingViews state. Currently used for history overlay and
// confirm bubble.
- (void)onOverlappedViewHidden;

// Executes the command registered by the extension that has the given id.
- (void)executeExtensionCommand:(const std::string&)extension_id
                        command:(const extensions::Command&)command;

// Activates the page action for the extension that has the given id.
- (void)activatePageAction:(const std::string&)extension_id;

// Activates the browser action for the extension that has the given id.
- (void)activateBrowserAction:(const std::string&)extension_id;
- (BOOL)userWillWaitForInProgressDownloadsChampion:(int)downloadCount; // champion : WBQ - Download is in progress warning dialog  (Balaji) 25July14

@end  // @interface BrowserWindowController


// Methods having to do with the window type (normal/popup/app, and whether the
// window has various features; fullscreen and presentation mode methods are
// separate).
@interface BrowserWindowController(WindowType)

// Determines whether this controller's window supports a given feature (i.e.,
// whether a given feature is or can be shown in the window).
// TODO(viettrungluu): |feature| is really should be |Browser::Feature|, but I
// don't want to include browser.h (and you can't forward declare enums).
- (BOOL)supportsWindowFeature:(int)feature;

// Called to check whether or not this window has a normal title bar (YES if it
// does, NO otherwise). (E.g., normal browser windows do not, pop-ups do.)
- (BOOL)hasTitleBar;

// Called to check whether or not this window has a toolbar (YES if it does, NO
// otherwise). (E.g., normal browser windows do, pop-ups do not.)
- (BOOL)hasToolbar;

// Called to check whether or not this window has a location bar (YES if it
// does, NO otherwise). (E.g., normal browser windows do, pop-ups may or may
// not.)
- (BOOL)hasLocationBar;

// Called to check whether or not this window can have bookmark bar (YES if it
// does, NO otherwise). (E.g., normal browser windows may, pop-ups may not.)
- (BOOL)supportsBookmarkBar;

// Called to check if this controller's window is a tabbed window (e.g., not a
// pop-up window). Returns YES if it is, NO otherwise.
// Note: The |-has...| methods are usually preferred, so this method is largely
// deprecated.
- (BOOL)isTabbedWindow;

@end  // @interface BrowserWindowController(WindowType)


// Methods having to do with fullscreen and presentation mode.
@interface BrowserWindowController(Fullscreen)

// Toggles fullscreen mode.  Meant to be called by Lion windows when they enter
// or exit Lion fullscreen mode.  Must not be called on Snow Leopard or earlier.
- (void)handleLionToggleFullscreen;

// Enters (or exits) fullscreen mode.  This method is safe to call on all OS
// versions.
- (void)enterFullscreen;
- (void)exitFullscreen;

// Updates the contents of the fullscreen exit bubble with |url| and
// |bubbleType|.
- (void)updateFullscreenExitBubbleURL:(const GURL&)url
                           bubbleType:(FullscreenExitBubbleType)bubbleType;

// Returns fullscreen state: YES when the window is in fullscreen or is
// animating into fullscreen.
- (BOOL)isFullscreen;

// Returns YES if the browser window is currently in fullscreen via the built-in
// immersive mechanism.
- (BOOL)isInImmersiveFullscreen;

// Returns YES if the browser window is currently in fullscreen via the Cocoa
// System Fullscreen API.
- (BOOL)isInSystemFullscreen;

// Enters (or exits) presentation mode.  Also enters fullscreen mode if this
// window is not already fullscreen.  This method is safe to call on all OS
// versions.
- (void)enterPresentationModeForURL:(const GURL&)url
                         bubbleType:(FullscreenExitBubbleType)bubbleType;
- (void)exitPresentationMode;

// For simplified fullscreen: Enters fullscreen for a tab at a URL. The |url|
// is guaranteed to be non-empty; see -enterFullscreen for the user-initiated
// fullscreen mode. Called on Snow Leopard and Lion+.
- (void)enterFullscreenForURL:(const GURL&)url
                   bubbleType:(FullscreenExitBubbleType)bubbleType;

// Returns presentation mode state.  This method is safe to call on all OS
// versions.
- (BOOL)inPresentationMode;

// Resizes the fullscreen window to fit the screen it's currently on.  Called by
// the PresentationModeController when there is a change in monitor placement or
// resolution.
- (void)resizeFullscreenWindow;

// Gets or sets the fraction of the floating bar (presentation mode overlay)
// that is shown.  0 is completely hidden, 1 is fully shown.
- (CGFloat)floatingBarShownFraction;
- (void)setFloatingBarShownFraction:(CGFloat)fraction;

// Query/lock/release the requirement that the tab strip/toolbar/attached
// bookmark bar bar cluster is visible (e.g., when one of its elements has
// focus). This is required for the floating bar in presentation mode, but
// should also be called when not in presentation mode; see the comments for
// |barVisibilityLocks_| for more details. Double locks/releases by the same
// owner are ignored. If |animate:| is YES, then an animation may be performed,
// possibly after a small delay if |delay:| is YES. If |animate:| is NO,
// |delay:| will be ignored. In the case of multiple calls, later calls have
// precedence with the rule that |animate:NO| has precedence over |animate:YES|,
// and |delay:NO| has precedence over |delay:YES|.
- (BOOL)isBarVisibilityLockedForOwner:(id)owner;
- (void)lockBarVisibilityForOwner:(id)owner
                    withAnimation:(BOOL)animate
                            delay:(BOOL)delay;
- (void)releaseBarVisibilityForOwner:(id)owner
                       withAnimation:(BOOL)animate
                               delay:(BOOL)delay;

// Returns YES if any of the views in the floating bar currently has focus.
- (BOOL)floatingBarHasFocus;

@end  // @interface BrowserWindowController(Fullscreen)


// Methods which are either only for testing, or only public for testing.
@interface BrowserWindowController (TestingAPI)

// Put the incognito badge or multi-profile avatar on the browser and adjust the
// tab strip accordingly.
- (void)installAvatar;

// Allows us to initWithBrowser withOUT taking ownership of the browser.
- (id)initWithBrowser:(Browser*)browser takeOwnership:(BOOL)ownIt;

// Adjusts the window height by the given amount.  If the window spans from the
// top of the current workspace to the bottom of the current workspace, the
// height is not adjusted.  If growing the window by the requested amount would
// size the window to be taller than the current workspace, the window height is
// capped to be equal to the height of the current workspace.  If the window is
// partially offscreen, its height is not adjusted at all.  This function
// prefers to grow the window down, but will grow up if needed.  Calls to this
// function should be followed by a call to |layoutSubviews|.
// Returns if the window height was changed.
- (BOOL)adjustWindowHeightBy:(CGFloat)deltaH;

// Return an autoreleased NSWindow suitable for fullscreen use.
- (NSWindow*)createFullscreenWindow;

// Resets any saved state about window growth (due to showing the bookmark bar
// or the download shelf), so that future shrinking will occur from the bottom.
- (void)resetWindowGrowthState;

// Computes by how far in each direction, horizontal and vertical, the
// |source| rect doesn't fit into |target|.
- (NSSize)overflowFrom:(NSRect)source
                    to:(NSRect)target;

// The fullscreen exit bubble controller, or nil if the bubble isn't showing.
- (FullscreenExitBubbleController*)fullscreenExitBubbleController;

// Gets the rect, in window base coordinates, that the omnibox popup should be
// positioned relative to.
- (NSRect)omniboxPopupAnchorRect;

// Force a layout of info bars.
- (void)layoutInfoBars;

@end  // @interface BrowserWindowController (TestingAPI)


#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_CONTROLLER_H_
