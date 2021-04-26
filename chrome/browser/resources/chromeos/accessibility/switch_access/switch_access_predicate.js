// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const StateType = chrome.automation.StateType;
const RoleType = chrome.automation.RoleType;
const DefaultActionVerb = chrome.automation.DefaultActionVerb;

/**
 * Contains predicates for the chrome automation API. The following basic
 * predicates are available:
 *    - isActionable
 *    - isGroup
 *    - isInteresting
 *    - isInterestingSubtree
 *    - isVisible
 *    - isTextInput
 *
 * In addition to these basic predicates, there are also methods to get the
 * restrictions required by TreeWalker for specific traversal situations.
 */
const SwitchAccessPredicate = {
  GROUP_INTERESTING_CHILD_THRESHOLD: 2,

  /**
   * Returns true if |node| is actionable, meaning that a user can interact with
   * it in some way.
   *
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  isActionable: (node) => {
    const defaultActionVerb = node.defaultActionVerb;
    const loc = node.location;
    const parent = node.parent;
    const root = node.root;
    const role = node.role;
    const state = node.state;

    // Skip things that are offscreen or invisible.
    if (!SwitchAccessPredicate.isVisible(node)) {
      return false;
    }

    // Skip things that are disabled.
    if (node.restriction === chrome.automation.Restriction.DISABLED) {
      return false;
    }

    // These web containers are not directly actionable.
    if (role === RoleType.WEB_VIEW || role === RoleType.ROOT_WEB_AREA) {
      return false;
    }

    if (parent) {
      // crbug.com/710559
      // Work around for browser tabs.
      if (role === RoleType.TAB && parent.role === RoleType.TAB_LIST &&
          root.role === RoleType.DESKTOP) {
        return true;
      }
    }

    // Check various indicators that the node is actionable.
    if (role === RoleType.BUTTON || role === RoleType.SLIDER) {
      return true;
    }

    if (AutomationPredicate.comboBox(node) ||
        SwitchAccessPredicate.isTextInput(node)) {
      return true;
    }

    if (defaultActionVerb &&
        (defaultActionVerb === DefaultActionVerb.ACTIVATE ||
         defaultActionVerb === DefaultActionVerb.CHECK ||
         defaultActionVerb === DefaultActionVerb.OPEN ||
         defaultActionVerb === DefaultActionVerb.PRESS ||
         defaultActionVerb === DefaultActionVerb.SELECT ||
         defaultActionVerb === DefaultActionVerb.UNCHECK)) {
      return true;
    }

    if (role === RoleType.LIST_ITEM &&
        defaultActionVerb === DefaultActionVerb.CLICK) {
      return true;
    }

    // Focusable items should be surfaced as either groups or actionable. So
    // should menu items.
    // Current heuristic is to show as actionble any focusable item where no
    // child is an interesting subtree.
    if (state[StateType.FOCUSABLE] || role === RoleType.MENU_ITEM) {
      return !node.children.some(SwitchAccessPredicate.isInterestingSubtree);
    }
    return false;
  },

  /**
   * Returns true if |node| is a group, meaning that the node has more than one
   * interesting descendant, and that its interesting descendants exist in more
   * than one subtree of its immediate children.
   *
   * Additionally, for |node| to be a group, it cannot have the same bounding
   * box as its scope.
   *
   * @param {!AutomationNode} node
   * @param {AutomationNode|SARootNode} scope
   * @return {boolean}
   */
  isGroup: (node, scope) => {
    const scopeEqualsNode = scope &&
        (scope instanceof SARootNode ? scope.isEquivalentTo(node) :
                                       scope === node);
    if (scope && !scopeEqualsNode &&
        RectHelper.areEqual(node.location, scope.location)) {
      return false;
    }
    if (node.state[StateType.INVISIBLE]) {
      return false;
    }
    if (AutomationPredicate.comboBox(node)) {
      return false;
    }

    let interestingBranchesCount =
        SwitchAccessPredicate.isActionable(node) ? 1 : 0;
    let child = node.firstChild;
    while (child) {
      if (SwitchAccessPredicate.isInterestingSubtree(child)) {
        interestingBranchesCount += 1;
      }
      if (interestingBranchesCount >=
          SwitchAccessPredicate.GROUP_INTERESTING_CHILD_THRESHOLD) {
        return true;
      }
      child = child.nextSibling;
    }
    return false;
  },

  /**
   * Returns true if |node| is interesting for the user, meaning that |node|
   * is either actionable or a group.
   *
   * @param {!AutomationNode} node
   * @param {!AutomationNode|!SARootNode} scope
   * @return {boolean}
   */
  isInteresting: (node, scope) => SwitchAccessPredicate.isActionable(node) ||
      SwitchAccessPredicate.isGroup(node, scope),

  /**
   * Returns true if the element is visible to the user for any reason.
   *
   * @param {AutomationNode} node
   * @return {boolean}
   */
  isVisible: (node) => !node.state[StateType.OFFSCREEN] && !!node.location &&
      node.location.top >= 0 && node.location.left >= 0 &&
      !node.state[StateType.INVISIBLE],

  /**
   * Returns true if there is an interesting node in the subtree containing
   * |node| as its root (including |node| itself).
   *
   * This function does not call isInteresting directly, because that would
   * cause a loop (isInteresting calls isGroup, and isGroup calls
   * isInterestingSubtree).
   *
   * @param {!AutomationNode} node
   * @return {boolean}
   */
  isInterestingSubtree: (node) => SwitchAccessPredicate.isActionable(node) ||
      node.children.some(SwitchAccessPredicate.isInterestingSubtree),

  /**
   * Returns true if |node| is an element that contains editable text.
   * @param {AutomationNode} node
   * @return {boolean}
   */
  isTextInput: (node) => !!node && !!node.state[StateType.EDITABLE],

  /**
   * Returns a Restrictions object ready to be passed to AutomationTreeWalker.
   *
   * @param {!AutomationNode} scope
   * @return {!AutomationTreeWalkerRestriction}
   */
  restrictions: (scope) => {
    return {
      leaf: SwitchAccessPredicate.leaf(scope),
      root: SwitchAccessPredicate.root(scope),
      visit: SwitchAccessPredicate.visit(scope)
    };
  },

  /**
   * Creates a function that confirms if |node| is a terminal leaf node of a
   * SwitchAccess scope tree when |scope| is the root.
   *
   * @param {!AutomationNode} scope
   * @return {function(!AutomationNode): boolean}
   */
  leaf(scope) {
    return (node) => node.state[StateType.INVISIBLE] ||
        !SwitchAccessPredicate.isInterestingSubtree(node) ||
        (scope !== node && SwitchAccessPredicate.isInteresting(node, scope));
  },

  /**
   * Creates a function that confirms if |node| is the root of a SwitchAccess
   * scope tree when |scope| is the root.
   *
   * @param {!AutomationNode} scope
   * @return {function(!AutomationNode): boolean}
   */
  root(scope) {
    return (node) => scope === node;
  },

  /**
   * Creates a function that determines whether |node| is to be visited in the
   * SwitchAccess scope tree with |scope| as the root.
   *
   * @param {!AutomationNode} scope
   * @return {function(!AutomationNode): boolean}
   */
  visit(scope) {
    return (node) => node.role !== RoleType.DESKTOP &&
        SwitchAccessPredicate.isInteresting(node, scope);
  }
};
