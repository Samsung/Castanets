// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle focus rings.
 */
class FocusRingManager {
  /** @private */
  constructor() {
    /**
     * A map of all the focus rings.
     * @private {!Map<SAConstants.Focus.ID,
     *     chrome.accessibilityPrivate.FocusRingInfo>}
     */
    this.rings_ = this.createMap_();

    /**
     * Regex pattern to verify valid colors. Checks that the first character
     * is '#', followed by 3, 4, 6, or 8 valid hex characters, and no other
     * characters (ignoring case).
     * @private
     */
    this.colorPattern_ = /^#([0-9A-F]{3,4}|[0-9A-F]{6}|[0-9A-F]{8})$/i;
  }

  static initialize() {
    if (!FocusRingManager.instance) {
      FocusRingManager.instance = new FocusRingManager();
    }
  }

  /**
   * Sets the focus ring color.
   * @param {!string} color
   */
  static setColor(color) {
    if (!FocusRingManager.instance) {
      FocusRingManager.initialize();
    }
    const manager = FocusRingManager.instance;

    if (manager.colorPattern_.test(color) !== true) {
      console.error(SwitchAccess.error(
          SAConstants.ErrorType.INVALID_COLOR,
          'Problem setting focus ring color: ' + color + ' is not' +
              'a valid CSS color string.'));
      return;
    }
    manager.rings_.forEach((ring) => ring.color = color);
  }

  /**
   * Sets the primary and next focus rings based on the current primary and
   *     group nodes used for navigation.
   * @param {!SAChildNode} node
   */
  static setFocusedNode(node) {
    if (!FocusRingManager.instance) {
      FocusRingManager.initialize();
    }
    const manager = FocusRingManager.instance;

    if (node instanceof BackButtonNode) {
      const backButton = /** @type {!BackButtonNode} */ (node);
      // The back button node handles setting its own focus, as it has special
      // requirements (a round focus ring that has no gap with the edges of the
      // view).
      manager.rings_.get(SAConstants.Focus.ID.PRIMARY).rects = [];
      // Clear the dashed ring between transitions, as the animation is
      // distracting.
      manager.rings_.get(SAConstants.Focus.ID.NEXT).rects = [];
      manager.updateFocusRings_();

      // The dashed focus ring should not be shown around the menu when exiting.
      if (!MenuManager.isMenuOpen()) {
        manager.rings_.get(SAConstants.Focus.ID.NEXT).rects =
            [backButton.group.location];
        manager.updateFocusRings_();
      }
      return;
    }

    if (!node.location) {
      setTimeout(NavigationManager.moveToValidNode, 0);
      throw SwitchAccess.error(
          SAConstants.ErrorType.MISSING_LOCATION,
          'Cannot set focus rings if node location is undefined');
    }

    // If the primary node is a group, show its first child as the "next" focus.
    if (node.isGroup()) {
      const firstChild = node.asRootNode().firstChild;

      // Clear the dashed ring between transitions, as the animation is
      // distracting.
      manager.rings_.get(SAConstants.Focus.ID.NEXT).rects = [];
      manager.updateFocusRings_();

      let focusRect = node.location;
      const childRect = firstChild ? firstChild.location : null;
      if (childRect) {
        // If the current element is not the back button, the focus rect should
        // expand to contain the child rect.
        focusRect = RectHelper.expandToFitWithPadding(
            SAConstants.Focus.GROUP_BUFFER, focusRect, childRect);
        manager.rings_.get(SAConstants.Focus.ID.NEXT).rects = [childRect];
      }
      manager.rings_.get(SAConstants.Focus.ID.PRIMARY).rects = [focusRect];
      manager.updateFocusRings_();
      return;
    }

    manager.rings_.get(SAConstants.Focus.ID.PRIMARY).rects = [node.location];
    manager.rings_.get(SAConstants.Focus.ID.NEXT).rects = [];
    manager.updateFocusRings_();
  }

  /** Clears all focus rings. */
  static clearAll() {
    if (!FocusRingManager.instance) {
      FocusRingManager.initialize();
    }
    const manager = FocusRingManager.instance;
    manager.rings_.forEach((ring) => ring.rects = []);
    manager.updateFocusRings_();
  }

  /**
   * Creates the map of focus rings.
   * @return {!Map<SAConstants.Focus.ID,
   * chrome.accessibilityPrivate.FocusRingInfo>}
   * @private
   */
  createMap_() {
    const primaryRing = {
      id: SAConstants.Focus.ID.PRIMARY,
      rects: [],
      type: chrome.accessibilityPrivate.FocusType.SOLID,
      color: SAConstants.Focus.PRIMARY_COLOR,
      secondaryColor: SAConstants.Focus.SECONDARY_COLOR
    };

    const nextRing = {
      id: SAConstants.Focus.ID.NEXT,
      rects: [],
      type: chrome.accessibilityPrivate.FocusType.DASHED,
      color: SAConstants.Focus.PRIMARY_COLOR,
      secondaryColor: SAConstants.Focus.SECONDARY_COLOR
    };

    return new Map([
      [SAConstants.Focus.ID.PRIMARY, primaryRing],
      [SAConstants.Focus.ID.NEXT, nextRing]
    ]);
  }


  /**
   * Updates all focus rings to reflect new location, color, style, or other
   * changes.
   * @private
   */
  updateFocusRings_() {
    const focusRings = [];
    this.rings_.forEach((ring) => focusRings.push(ring));
    chrome.accessibilityPrivate.setFocusRings(focusRings);
  }
}
