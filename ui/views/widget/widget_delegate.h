// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_DELEGATE_H_
#define UI_VIEWS_WIDGET_WIDGET_DELEGATE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class ImageSkia;
class Rect;
}  // namespace gfx

namespace views {
class BubbleDialogDelegate;
class ClientView;
class DialogDelegate;
class NonClientFrameView;
class View;

// Handles events on Widgets in context-specific ways.
class VIEWS_EXPORT WidgetDelegate {
 public:
  struct Params {
    Params();
    ~Params();

    // The window's role. Useful values include kWindow (a plain window),
    // kDialog (a dialog), and kAlertDialog (a high-priority dialog whose body
    // is read when it appears). Using a role outside this set is not likely to
    // work across platforms.
    ax::mojom::Role accessible_role = ax::mojom::Role::kWindow;

    // The accessible title for the window, often more descriptive than the
    // plain title. If no accessible title is present the result of
    // GetWindowTitle() will be used.
    base::string16 accessible_title;

    // Whether the window should display controls for the user to minimize,
    // maximize, or resize it.
    bool can_maximize = false;
    bool can_minimize = false;
    bool can_resize = false;

#if defined(USE_AURA)
    // Whether to center the widget's title within the frame.
    bool center_title = false;
#endif

    // Controls focus traversal past the first/last focusable view.
    // If true, focus moves out of this Widget and to this Widget's toplevel
    // Widget; if false, focus cycles within this Widget.
    bool focus_traverses_out = false;

    // The widget's icon, if any.
    gfx::ImageSkia icon;

    // Whether to show a close button in the widget frame.
    bool show_close_button = true;

    // Whether to show the widget's icon.
    // TODO(ellyjones): What if this was implied by !icon.isNull()?
    bool show_icon = false;

    // Whether to display the widget's title in the frame.
    bool show_title = true;

    // The widget's title, if any.
    // TODO(ellyjones): Should it be illegal to have show_title && !title?
    base::string16 title;
  };

  WidgetDelegate();
  virtual ~WidgetDelegate();

  // Sets the return value of CanActivate(). Default is true.
  void SetCanActivate(bool can_activate);

  // Called whenever the widget's position changes.
  virtual void OnWidgetMove();

  // Called with the display changes (color depth or resolution).
  virtual void OnDisplayChanged();

  // Called when the work area (the desktop area minus task bars,
  // menu bars, etc.) changes in size.
  virtual void OnWorkAreaChanged();

  // Called when the widget's initialization is beginning, right after the
  // ViewsDelegate decides to use this WidgetDelegate for a Widget.
  virtual void OnWidgetInitializing() {}

  // Called when the widget's initialization is complete.
  virtual void OnWidgetInitialized() {}

  // Called when the window has been requested to close, after all other checks
  // have run. Returns whether the window should be allowed to close (default is
  // true).
  //
  // Can be used as an alternative to specifying a custom ClientView with
  // the CanClose() method, or in widget types which do not support a
  // ClientView.
  virtual bool OnCloseRequested(Widget::ClosedReason close_reason);

  // Returns the view that should have the focus when the widget is shown.  If
  // NULL no view is focused.
  virtual View* GetInitiallyFocusedView();

  virtual BubbleDialogDelegate* AsBubbleDialogDelegate();
  virtual DialogDelegate* AsDialogDelegate();

  // Returns true if the window can be resized.
  virtual bool CanResize() const;

  // Returns true if the window can be maximized.
  virtual bool CanMaximize() const;

  // Returns true if the window can be minimized.
  virtual bool CanMinimize() const;

  // Returns true if the window can be activated.
  virtual bool CanActivate() const;

  // Returns the modal type that applies to the widget. Default is
  // ui::MODAL_TYPE_NONE (not modal).
  virtual ui::ModalType GetModalType() const;

  virtual ax::mojom::Role GetAccessibleWindowRole();

  // Returns the title to be read with screen readers.
  virtual base::string16 GetAccessibleWindowTitle() const;

  // Returns the text to be displayed in the window title.
  virtual base::string16 GetWindowTitle() const;

  // Returns true if the window should show a title in the title bar.
  virtual bool ShouldShowWindowTitle() const;

  // Returns true if the window should show a close button in the title bar.
  virtual bool ShouldShowCloseButton() const;

  // Returns the app icon for the window. On Windows, this is the ICON_BIG used
  // in Alt-Tab list and Win7's taskbar.
  virtual gfx::ImageSkia GetWindowAppIcon();

  // Returns the icon to be displayed in the window.
  virtual gfx::ImageSkia GetWindowIcon();

  // Returns true if a window icon should be shown.
  virtual bool ShouldShowWindowIcon() const;

  // Execute a command in the window's controller. Returns true if the command
  // was handled, false if it was not.
  virtual bool ExecuteWindowsCommand(int command_id);

  // Returns the window's name identifier. Used to identify this window for
  // state restoration.
  virtual std::string GetWindowName() const;

  // Saves the window's bounds and "show" state. By default this uses the
  // process' local state keyed by window name (See GetWindowName above). This
  // behavior can be overridden to provide additional functionality.
  virtual void SaveWindowPlacement(const gfx::Rect& bounds,
                                   ui::WindowShowState show_state);

  // Retrieves the window's bounds and "show" states.
  // This behavior can be overridden to provide additional functionality.
  virtual bool GetSavedWindowPlacement(const Widget* widget,
                                       gfx::Rect* bounds,
                                       ui::WindowShowState* show_state) const;

  // Returns true if the window's size should be restored. If this is false,
  // only the window's origin is restored and the window is given its
  // preferred size.
  // Default is true.
  virtual bool ShouldRestoreWindowSize() const;

  // Hooks for the end of the Widget/Window lifecycle. As of this writing, these
  // callbacks happen like so:
  //   1. Client code calls Widget::CloseWithReason()
  //   2. WidgetDelegate::WindowWillClose() is called
  //   3. NativeWidget teardown (maybe async) starts OR the operating system
  //      abruptly closes the backing native window
  //   4. WidgetDelegate::WindowClosing() is called
  //   5. NativeWidget teardown completes, Widget teardown starts
  //   6. WidgetDelegate::DeleteDelegate() is called
  //   7. Widget teardown finishes, Widget is deleted
  // At step 3, the "maybe async" is controlled by whether the close is done via
  // Close() or CloseNow().
  // Important note: for OS-initiated window closes, steps 1 and 2 don't happen
  // - i.e, WindowWillClose() is never invoked.
  //
  // The default implementations of these methods simply call the corresponding
  // callbacks; see Set*Callback() below. If you override these it is not
  // necessary to call the base implementations.
  virtual void WindowClosing();
  virtual void DeleteDelegate();

  // Called when the user begins/ends to change the bounds of the window.
  virtual void OnWindowBeginUserBoundsChange() {}
  virtual void OnWindowEndUserBoundsChange() {}

  // Returns the Widget associated with this delegate.
  virtual Widget* GetWidget();
  virtual const Widget* GetWidget() const;

  // Get the view that is contained within this widget.
  //
  // WARNING: This method has unusual ownership behavior:
  // * If the returned view is owned_by_client(), then the returned pointer is
  //   never an owning pointer;
  // * If the returned view is !owned_by_client() (the default & the
  //   recommendation), then the returned pointer is *sometimes* an owning
  //   pointer and sometimes not. Specifically, it is an owning pointer exactly
  //   once, when this method is being used to construct the ClientView, which
  //   takes ownership of the ContentsView() when !owned_by_client().
  //
  // Apart from being difficult to reason about this introduces a problem: a
  // WidgetDelegate can't know whether it owns its contents view or not, so
  // constructing a WidgetDelegate which one does not then use to construct a
  // Widget (often done in tests) leaks memory in a way that can't be locally
  // fixed.
  //
  // TODO(ellyjones): This is not tenable - figure out how this should work and
  // replace it.
  virtual View* GetContentsView();

  // Called by the Widget to create the Client View used to host the contents
  // of the widget.
  virtual ClientView* CreateClientView(Widget* widget);

  // Called by the Widget to create the NonClient Frame View for this widget.
  // Return NULL to use the default one.
  virtual NonClientFrameView* CreateNonClientFrameView(Widget* widget);

  // Called by the Widget to create the overlay View for this widget. Return
  // NULL for no overlay. The overlay View will fill the Widget and sit on top
  // of the ClientView and NonClientFrameView (both visually and wrt click
  // targeting).
  virtual View* CreateOverlayView();

  // Returns true if the window can be notified with the work area change.
  // Otherwise, the work area change for the top window will be processed by
  // the default window manager. In some cases, like panel, we would like to
  // manage the positions by ourselves.
  virtual bool WillProcessWorkAreaChange() const;

  // Returns true if window has a hit-test mask.
  virtual bool WidgetHasHitTestMask() const;

  // Provides the hit-test mask if HasHitTestMask above returns true.
  virtual void GetWidgetHitTestMask(SkPath* mask) const;

  // Returns true if event handling should descend into |child|.
  // |location| is in terms of the Window.
  virtual bool ShouldDescendIntoChildForEventHandling(
      gfx::NativeView child,
      const gfx::Point& location);

  // Populates |panes| with accessible panes in this window that can
  // be cycled through with keyboard focus.
  virtual void GetAccessiblePanes(std::vector<View*>* panes) {}

  // Setters for data parameters of the WidgetDelegate. If you use these
  // setters, there is no need to override the corresponding virtual getters.
  void SetAccessibleRole(ax::mojom::Role role);
  void SetAccessibleTitle(base::string16 title);
  void SetCanMaximize(bool can_maximize);
  void SetCanMinimize(bool can_minimize);
  void SetCanResize(bool can_resize);
  void SetFocusTraversesOut(bool focus_traverses_out);
  void SetIcon(const gfx::ImageSkia& icon);
  void SetShowCloseButton(bool show_close_button);
  void SetShowIcon(bool show_icon);
  void SetShowTitle(bool show_title);
  void SetTitle(const base::string16& title);
  void SetTitle(int title_message_id);
#if defined(USE_AURA)
  void SetCenterTitle(bool center_title);
#endif

  // A convenience wrapper that does all three of SetCanMaximize,
  // SetCanMinimize, and SetCanResize.
  void SetHasWindowSizeControls(bool has_controls);

  void RegisterWindowWillCloseCallback(base::OnceClosure callback);
  void RegisterWindowClosingCallback(base::OnceClosure callback);
  void RegisterDeleteDelegateCallback(base::OnceClosure callback);

  // Called to notify the WidgetDelegate of changes to the state of its Widget.
  // It is not usually necessary to call these from client code.
  void WidgetInitializing(Widget* widget);
  void WidgetInitialized();
  void WidgetDestroying();
  void WindowWillClose();

  // Returns true if the title text should be centered.
  bool ShouldCenterWindowTitleText() const;

  bool focus_traverses_out() const { return params_.focus_traverses_out; }

 private:
  friend class Widget;

  // The Widget that was initialized with this instance as its WidgetDelegate,
  // if any.
  Widget* widget_ = nullptr;
  Params params_;

  View* default_contents_view_ = nullptr;
  bool can_activate_ = true;

  // Managed by Widget. Ensures |this| outlives its Widget.
  bool can_delete_this_ = true;

  std::vector<base::OnceClosure> window_will_close_callbacks_;
  std::vector<base::OnceClosure> window_closing_callbacks_;
  std::vector<base::OnceClosure> delete_delegate_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(WidgetDelegate);
};

// A WidgetDelegate implementation that is-a View. Used to override GetWidget()
// to call View's GetWidget() for the common case where a WidgetDelegate
// implementation is-a View. Note that WidgetDelegateView is not owned by
// view's hierarchy and is expected to be deleted on DeleteDelegate call.
class VIEWS_EXPORT WidgetDelegateView : public WidgetDelegate, public View {
 public:
  METADATA_HEADER(WidgetDelegateView);

  WidgetDelegateView();
  ~WidgetDelegateView() override;

  // WidgetDelegate:
  void DeleteDelegate() override;
  Widget* GetWidget() override;
  const Widget* GetWidget() const override;
  View* GetContentsView() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WidgetDelegateView);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WIDGET_DELEGATE_H_
