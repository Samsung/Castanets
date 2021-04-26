// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class handles interactions with an onscreen element based on a single
 * AutomationNode.
 */
class NodeWrapper extends SAChildNode {
  /**
   * @param {!AutomationNode} baseNode
   * @param {?SARootNode} parent
   * @protected
   */
  constructor(baseNode, parent) {
    super();
    /** @private {!AutomationNode} */
    this.baseNode_ = baseNode;

    /** @private {?SARootNode} */
    this.parent_ = parent;

    /** @private {boolean} */
    this.isGroup_ = SwitchAccessPredicate.isGroup(this.baseNode_, parent);

    /** @private {RepeatedEventHandler} */
    this.locationChangedHandler_;
  }

  // ================= Getters and setters =================

  /** @override */
  get actions() {
    const actions = [];
    actions.push(SwitchAccessMenuAction.SELECT);

    const ancestor = this.getScrollableAncestor_();
    if (ancestor.scrollable) {
      if (ancestor.scrollX > ancestor.scrollXMin) {
        actions.push(SwitchAccessMenuAction.SCROLL_LEFT);
      }
      if (ancestor.scrollX < ancestor.scrollXMax) {
        actions.push(SwitchAccessMenuAction.SCROLL_RIGHT);
      }
      if (ancestor.scrollY > ancestor.scrollYMin) {
        actions.push(SwitchAccessMenuAction.SCROLL_UP);
      }
      if (ancestor.scrollY < ancestor.scrollYMax) {
        actions.push(SwitchAccessMenuAction.SCROLL_DOWN);
      }
    }
    const standardActions = /** @type {!Array<!SwitchAccessMenuAction>} */ (
        this.baseNode_.standardActions.filter(
            action => Object.values(SwitchAccessMenuAction).includes(action)));

    return actions.concat(standardActions);
  }

  /** @override */
  get automationNode() {
    return this.baseNode_;
  }

  /** @override */
  get location() {
    return this.baseNode_.location;
  }

  /** @override */
  get role() {
    return this.baseNode_.role;
  }

  // ================= General methods =================

  /** @override */
  asRootNode() {
    if (!this.isGroup()) {
      return null;
    }
    return RootNodeWrapper.buildTree(this.baseNode_);
  }

  /** @override */
  equals(other) {
    if (!other || !(other instanceof NodeWrapper)) {
      return false;
    }

    other = /** @type {!NodeWrapper} */ (other);
    return other.baseNode_ === this.baseNode_;
  }

  /** @override */
  isEquivalentTo(node) {
    if (node instanceof NodeWrapper || node instanceof RootNodeWrapper) {
      return this.baseNode_ === node.baseNode_;
    }

    if (node instanceof SAChildNode) {
      return node.isEquivalentTo(this);
    }
    return this.baseNode_ === node;
  }

  /** @override */
  isGroup() {
    return this.isGroup_;
  }

  /** @override */
  isValidAndVisible() {
    // Nodes without a role are not valid.
    if (!this.baseNode_.role) {
      return false;
    }
    return SwitchAccessPredicate.isVisible(this.baseNode_) &&
        super.isValidAndVisible();
  }

  /** @override */
  onFocus() {
    super.onFocus();
    this.locationChangedHandler_ = new RepeatedEventHandler(
        this.baseNode_, chrome.automation.EventType.LOCATION_CHANGED,
        () => FocusRingManager.setFocusedNode(this));
  }

  /** @override */
  onUnfocus() {
    super.onUnfocus();
    if (this.locationChangedHandler_) {
      this.locationChangedHandler_.stopListening();
    }
  }

  /** @override */
  performAction(action) {
    let ancestor;
    switch (action) {
      case SwitchAccessMenuAction.SELECT:
        if (this.isGroup()) {
          NavigationManager.enterGroup();
        } else {
          this.baseNode_.doDefault();
        }
        return SAConstants.ActionResponse.CLOSE_MENU;
      case SwitchAccessMenuAction.SCROLL_DOWN:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) {
          ancestor.scrollDown(() => this.parent_.refresh());
        }
        return SAConstants.ActionResponse.RELOAD_MAIN_MENU;
      case SwitchAccessMenuAction.SCROLL_UP:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) {
          ancestor.scrollUp(() => this.parent_.refresh());
        }
        return SAConstants.ActionResponse.RELOAD_MAIN_MENU;
      case SwitchAccessMenuAction.SCROLL_RIGHT:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) {
          ancestor.scrollRight(() => this.parent_.refresh());
        }
        return SAConstants.ActionResponse.RELOAD_MAIN_MENU;
      case SwitchAccessMenuAction.SCROLL_LEFT:
        ancestor = this.getScrollableAncestor_();
        if (ancestor.scrollable) {
          ancestor.scrollLeft(() => this.parent_.refresh());
        }
        return SAConstants.ActionResponse.RELOAD_MAIN_MENU;
      default:
        if (Object.values(chrome.automation.ActionType).includes(action)) {
          this.baseNode_.performStandardAction(
              /** @type {chrome.automation.ActionType} */ (action));
        }
        return SAConstants.ActionResponse.CLOSE_MENU;
    }
  }

  // ================= Private methods =================

  /**
   * @return {AutomationNode}
   * @protected
   */
  getScrollableAncestor_() {
    let ancestor = this.baseNode_;
    while (!ancestor.scrollable && ancestor.parent) {
      ancestor = ancestor.parent;
    }
    return ancestor;
  }

  // ================= Static methods =================

  /**
   * @param {!AutomationNode} baseNode
   * @param {?SARootNode} parent
   * @return {!NodeWrapper}
   */
  static create(baseNode, parent) {
    if (AutomationPredicate.comboBox(baseNode)) {
      return new ComboBoxNode(baseNode, parent);
    }
    if (SwitchAccessPredicate.isTextInput(baseNode)) {
      return new EditableTextNode(baseNode, parent);
    }

    switch (baseNode.role) {
      case chrome.automation.RoleType.SLIDER:
        return new SliderNode(baseNode, parent);
      case chrome.automation.RoleType.TAB:
        return TabNode.create(baseNode, parent);
      default:
        return new NodeWrapper(baseNode, parent);
    }
  }
}

/**
 * This class handles constructing and traversing a group of onscreen elements
 * based on all the interesting descendants of a single AutomationNode.
 */
class RootNodeWrapper extends SARootNode {
  /**
   * WARNING: If you call this constructor, you must *explicitly* set children.
   *     Use the static function RootNodeWrapper.buildTree for most use cases.
   * @param {!AutomationNode} baseNode
   */
  constructor(baseNode) {
    super();

    /** @private {!AutomationNode} */
    this.baseNode_ = baseNode;

    /** @private {boolean} */
    this.invalidated_ = false;

    /** @private {RepeatedEventHandler} */
    this.childrenChangedHandler_;
  }

  // ================= Getters and setters =================

  /** @override */
  get automationNode() {
    return this.baseNode_;
  }

  /** @override */
  get location() {
    return this.baseNode_.location || super.location;
  }

  // ================= General methods =================

  /** @override */
  equals(other) {
    if (!(other instanceof RootNodeWrapper)) {
      return false;
    }

    other = /** @type {!RootNodeWrapper} */ (other);
    return super.equals(other) && this.baseNode_ === other.baseNode_;
  }

  /** @override */
  isEquivalentTo(node) {
    if (node instanceof RootNodeWrapper || node instanceof NodeWrapper) {
      return this.baseNode_ === node.baseNode_;
    }

    if (node instanceof SAChildNode) {
      return node.isEquivalentTo(this);
    }
    return this.baseNode_ === node;
  }

  /** @override */
  isValidGroup() {
    if (!this.baseNode_.role) {
      // If the underlying automation node has been invalidated, return false.
      return false;
    }
    return !this.invalidated_ && super.isValidGroup();
  }

  /** @override */
  onFocus() {
    super.onFocus();
    this.childrenChangedHandler_ = new RepeatedEventHandler(
        this.baseNode_, chrome.automation.EventType.CHILDREN_CHANGED,
        this.refresh.bind(this));
  }

  /** @override */
  onUnfocus() {
    super.onUnfocus();
    if (this.childrenChangedHandler_) {
      this.childrenChangedHandler_.stopListening();
    }
  }

  /** @override */
  refresh() {
    // Find the currently focused child.
    let focusedChild = null;
    for (const child of this.children) {
      if (child.isFocused()) {
        focusedChild = child;
        break;
      }
    }

    // Update this RootNodeWrapper's children.
    const childConstructor = (node) => NodeWrapper.create(node, this);
    try {
      RootNodeWrapper.findAndSetChildren(this, childConstructor);
    } catch (e) {
      this.onUnfocus();
      this.invalidated_ = true;
      NavigationManager.moveToValidNode();
      return;
    }

    // Set the new instance of that child to be the focused node.
    for (const child of this.children) {
      if (child.isEquivalentTo(focusedChild)) {
        NavigationManager.forceFocusedNode(child);
        return;
      }
    }

    // If we didn't find a match, fall back and reset.
    NavigationManager.moveToValidNode();
  }

  // ================= Static methods =================

  /**
   * @param {!AutomationNode} rootNode
   * @return {!RootNodeWrapper}
   */
  static buildTree(rootNode) {
    if (rootNode.role === chrome.automation.RoleType.WINDOW ||
        (rootNode.role === chrome.automation.RoleType.CLIENT &&
         rootNode.parent.role === chrome.automation.RoleType.WINDOW)) {
      return WindowRootNode.buildTree(rootNode);
    }

    const root = new RootNodeWrapper(rootNode);
    const childConstructor = (node) => NodeWrapper.create(node, root);

    RootNodeWrapper.findAndSetChildren(root, childConstructor);
    return root;
  }

  /**
   * Helper function to connect tree elements, given the root node and a
   * constructor for the child type.
   * @param {!RootNodeWrapper} root
   * @param {function(!AutomationNode): !SAChildNode} childConstructor
   *     Constructs a child node from an automation node.
   */
  static findAndSetChildren(root, childConstructor) {
    const interestingChildren = RootNodeWrapper.getInterestingChildren(root);

    if (interestingChildren.length < 1) {
      setTimeout(NavigationManager.moveToValidNode, 0);
      throw SwitchAccess.error(
          SAConstants.ErrorType.NO_CHILDREN,
          'Root node must have at least 1 interesting child.');
    }
    const children = interestingChildren.map(childConstructor);
    children.push(new BackButtonNode(root));
    root.children = children;
  }

  /**
   * @param {!RootNodeWrapper|!AutomationNode} root
   * @return {!Array<!AutomationNode>}
   */
  static getInterestingChildren(root) {
    if (root instanceof RootNodeWrapper) {
      root = root.baseNode_;
    }

    if (root.children.length === 0) {
      return [];
    }
    const interestingChildren = [];
    const treeWalker = new AutomationTreeWalker(
        root, constants.Dir.FORWARD, SwitchAccessPredicate.restrictions(root));
    let node = treeWalker.next().node;

    while (node) {
      interestingChildren.push(node);
      node = treeWalker.next().node;
    }

    return interestingChildren;
  }
}
