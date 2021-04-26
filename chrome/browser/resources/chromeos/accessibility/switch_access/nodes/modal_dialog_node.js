// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** This class represents the group rooted at a modal dialog. */
class ModalDialogRootNode extends RootNodeWrapper {
  /** @override */
  onExit() {
    // To close a modal dialog, we need to send an escape key event.
    EventHelper.simulateKeyPress(EventHelper.KeyCode.ESC);
  }

  /**
   * Creates the tree structure for a modal dialog.
   * @param {!AutomationNode} dialogNode
   * @return {!ModalDialogRootNode}
   */
  static buildTree(dialogNode) {
    const root = new ModalDialogRootNode(dialogNode);
    const childConstructor = (node) => NodeWrapper.create(node, root);

    RootNodeWrapper.findAndSetChildren(root, childConstructor);
    return root;
  }
}
