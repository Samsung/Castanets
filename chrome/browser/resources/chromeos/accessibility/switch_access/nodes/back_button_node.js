// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class handles the behavior of the back button.
 */
class BackButtonNode extends SAChildNode {
  /**
   * @param {!SARootNode} group
   */
  constructor(group) {
    super();
    /**
     * The group that the back button is shown for.
     * @private {!SARootNode}
     */
    this.group_ = group;

    /** @private {function(chrome.automation.AutomationEvent)} */
    this.locationChangedHandler_ = () => FocusRingManager.setFocusedNode(this);
  }

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    return [SwitchAccessMenuAction.SELECT];
  }

  /** @override */
  get automationNode() {
    return BackButtonNode.automationNode_;
  }

  /** @return {!SARootNode} */
  get group() {
    return this.group_;
  }

  /** @override */
  get location() {
    if (BackButtonNode.locationForTesting) {
      return BackButtonNode.locationForTesting;
    }
    if (this.automationNode) {
      return this.automationNode.location;
    }
  }

  /** @override */
  get role() {
    return chrome.automation.RoleType.BUTTON;
  }

  // ================= General methods =================

  /** @override */
  asRootNode() {
    return null;
  }

  /** @override */
  equals(other) {
    return other instanceof BackButtonNode;
  }

  /** @override */
  isEquivalentTo(node) {
    return node instanceof BackButtonNode || this.automationNode === node;
  }

  /** @override */
  isGroup() {
    return false;
  }

  /** @override */
  onFocus() {
    super.onFocus();
    chrome.accessibilityPrivate.updateSwitchAccessBubble(
        chrome.accessibilityPrivate.SwitchAccessBubble.BACK_BUTTON,
        true /* show */, this.group_.location);
    BackButtonNode.findAutomationNode_();

    if (this.group_.automationNode) {
      this.group_.automationNode.addEventListener(
          chrome.automation.EventType.LOCATION_CHANGED,
          this.locationChangedHandler_, false /* is_capture */);
    }
  }

  /** @override */
  onUnfocus() {
    super.onUnfocus();
    chrome.accessibilityPrivate.updateSwitchAccessBubble(
        chrome.accessibilityPrivate.SwitchAccessBubble.BACK_BUTTON,
        false /* show */);

    if (this.group_.automationNode) {
      this.group_.automationNode.removeEventListener(
          chrome.automation.EventType.LOCATION_CHANGED,
          this.locationChangedHandler_, false /* is_capture */);
    }
  }

  /** @override */
  performAction(action) {
    if (action === SwitchAccessMenuAction.SELECT && this.automationNode) {
      BackButtonNode.onClick_();
      return SAConstants.ActionResponse.CLOSE_MENU;
    }
    return SAConstants.ActionResponse.NO_ACTION_TAKEN;
  }

  // ================= Debug methods =================

  /** @override */
  debugString() {
    return 'BackButtonNode';
  }

  // ================= Static methods =================

  /**
   * Looks for the back button automation node.
   * @private
   */
  static findAutomationNode_() {
    if (BackButtonNode.automationNode_ && BackButtonNode.automationNode_.role) {
      return;
    }
    SwitchAccess.findNodeMatchingPredicate(
        BackButtonNode.isBackButton_, BackButtonNode.saveAutomationNode_);
  }

  /**
   * Checks if the given node is the back button automation node.
   * @param {!AutomationNode} node
   * @return {boolean}
   * @private
   */
  static isBackButton_(node) {
    return node.htmlAttributes.id === 'switch_access_back_button';
  }

  /**
   * This function defines the behavior that should be taken when the back
   * button is pressed.
   * @private
   */
  static onClick_() {
    if (MenuManager.isMenuOpen()) {
      MenuManager.exit();
    } else {
      NavigationManager.exitGroupUnconditionally();
    }
  }

  /**
   * Saves the back button automation node.
   * @param {!AutomationNode} automationNode
   * @private
   */
  static saveAutomationNode_(automationNode) {
    BackButtonNode.automationNode_ = automationNode;
    BackButtonNode.automationNode_.addEventListener(
        chrome.automation.EventType.CLICKED, BackButtonNode.onClick_, false);
  }
}
