// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Data structures for representing a directed graph.
 */

/** Some aspects of the node's state, to help with node visualization. */
class NodeVisualizationState {
  constructor() {
    /** @public {boolean} */
    this.selectedByFilter = false;
    /** @public {boolean} */
    this.selectedByInbound = false;
    /** @public {boolean} */
    this.selectedByOutbound = false;
    /** @public {number} */
    this.inboundDepth = 0;
    /** @public {number} */
    this.outboundDepth = 0;
  }
}

/** A node in a directed graph. */
class Node {
  /**
   * @param {string} id The unique ID for the node.
   * @param {string} displayName The name to display when visualizing this node.
   */
  constructor(id, displayName) {
    /** @public @const {string} */
    this.id = id;
    /** @public @const {string} */
    this.displayName = displayName;

    // Sets a mutable state to help with visualizing the node.
    this.visualizationState = new NodeVisualizationState();

    // This information (edges) already exists in Graph.edges. However, we
    // duplicate it here to make BFS traversal more efficient.
    /** @public {!Set<!Node>} */
    this.inbound = new Set();
    /** @public {!Set<!Node>} */
    this.outbound = new Set();
  }

  /**
   * Resets the mutable visualization state back to its default.
   */
  resetVisualizationState() {
    this.visualizationState = new NodeVisualizationState();
  }

  /**
   * Adds a node to the inbound set of this node.
   * @param {!Node} other The inbound node.
   */
  addInbound(other) {
    this.inbound.add(other);
  }

  /**
   * Adds a node to the outbound set of this node.
   * @param {!Node} other The outbound node.
   */
  addOutbound(other) {
    this.outbound.add(other);
  }
}

/**
 * An edge in a directed graph.
 */
class Edge {
  /**
   * @param {string} id The unique ID for the edge.
   * @param {!Node} source The source Node object.
   * @param {!Node} target The target Node object.
   */
  constructor(id, source, target) {
    /** @public @const {string} */
    this.id = id;
    /** @public @const {!Node} */
    this.source = source;
    /** @public @const {!Node} */
    this.target = target;
  }
}

/**
 * The graph data for d3 to visualize.
 *
 * @typedef {Object} D3GraphData
 * @property {!Array<!Node>} nodes The nodes to visualize.
 * @property {!Array<!Edge>} edges The edges to visualize.
 */
let D3GraphData;

/**
 * Generates and returns a unique edge ID from its source/target Node IDs.
 * @param {string} sourceId The ID of the source node.
 * @param {string} targetId The ID of the target node.
 * @return {string} The ID uniquely identifying the edge source -> target.
 */
function getEdgeIdFromNodes(sourceId, targetId) {
  return `${sourceId} > ${targetId}`;
}

/** A directed graph. */
class GraphModel {
  constructor() {
    /** @public {!Map<string, !Node>} */
    this.nodes = new Map();
    /** @public {!Map<string, !Edge>} */
    this.edges = new Map();
  }

  /**
   * Adds a Node to the node set.
   * @param {!Node} node The node to add.
   */
  addNodeIfNew(node) {
    if (!this.nodes.has(node.id)) {
      this.nodes.set(node.id, node);
    }
  }

  /**
   * Retrieves a Node from the node set, if it exists.
   * @param {string} id The ID of the desired node.
   * @return {?Node} The Node if it exists, otherwise null.
   */
  getNodeById(id) {
    return this.nodes.get(id) || null;
  }

  /**
   * Retrieves an Edge from the edge set, if it exists.
   * @param {string} id The ID of the desired edge.
   * @return {?Edge} The Edge if it exists, otherwise null.
   */
  getEdgeById(id) {
    return this.edges.get(id) || null;
  }

  /**
   * Creates and adds an Edge to the edge set.
   * Also updates the inbound/outbound sets of the edge's nodes.
   * @param {!Node} sourceNode The node at the start of the edge.
   * @param {!Node} targetNode The node at the end of the edge.
   */
  addEdgeIfNew(sourceNode, targetNode) {
    const edgeId = getEdgeIdFromNodes(sourceNode.id, targetNode.id);
    if (!this.edges.has(edgeId)) {
      const edge = new Edge(edgeId, sourceNode, targetNode);
      this.edges.set(edgeId, edge);
      sourceNode.addOutbound(targetNode);
      targetNode.addInbound(sourceNode);
    }
  }

  /**
   * Generates the lists of nodes and edges for visualization with d3.
   *
   * The filter has three params: includedNodes (set of nodes), inbound (num),
   * and outbound (num). For nodes, we display `includedNodes` + the nodes
   * reachable within `inbound` inbound edges from `includedNodes` + the nodes
   * reachable within `outbound` outbound edges from `includedNodes`. For edges,
   * we display edges between all nodes except for the outermost layer, where we
   * only display the edges used to reach it from the second-outermost layer.
   *
   * @param {!GraphStore} filter The filter to apply to the data set.
   * @return {!D3GraphData} The nodes and edges to visualize.
   */
  getDataForD3(filter) {
    // These will be updated throughout the function and returned at the end.
    const /** !Set<!Node> */ resultNodeSet = new Set();
    const /** !Set<!Edge> */ resultEdgeSet = new Set();

    // Initialize the inbound and outbound BFS by setting the "seen" collection
    // to the filter nodes. We maintain both a Set and array for efficiency.
    const /** !Set<string> */ inboundSeenNodes = new Set(
        filter.includedNodeSet);
    const /** !Set<string> */ outboundSeenNodes = new Set(
        filter.includedNodeSet);
    const /** !Array<!Node> */ inboundNodeQueue = [];
    const /** !Array<!Node> */ outboundNodeQueue = [];
    for (const node of this.nodes.values()) {
      node.resetVisualizationState();
      if (filter.includedNodeSet.has(node.id)) {
        node.visualizationState.selectedByFilter = true;
        inboundNodeQueue.push(node);
        outboundNodeQueue.push(node);
        resultNodeSet.add(node);
      }
    }

    /**
     * Runs BFS and updates the result (resultNodeSet, resultEdgeSet).
     * @param {boolean} inboundTraversal Whether inbound edges should be used to
     *     traverse. If false, outbound edges are used.
     * @param {!Set<string>} seenNodes The IDs of nodes already visited in the
     *     BFS. Will be modified.
     * @param {!Array<!Node>} nodeQueue The queue used in BFS. Will be modified.
     * @param {number} maxDepth The depth of the traversal.
     */
    const updateResultBFS = (
        inboundTraversal, seenNodes, nodeQueue, maxDepth) => {
      while (nodeQueue.length > 0) {
        // The performance on the repeated `shift()`s is not as bad as it might
        // seem, since we only have ~500 nodes for the package graph.
        const curNode = nodeQueue.shift();
        const curDepth = inboundTraversal ?
          curNode.visualizationState.inboundDepth :
          curNode.visualizationState.outboundDepth;
        if (curDepth < maxDepth) {
          const otherNodes = inboundTraversal ?
            curNode.inbound : curNode.outbound;
          for (const otherNode of otherNodes) {
            if (!seenNodes.has(otherNode.id)) {
              if (inboundTraversal) {
                otherNode.visualizationState.selectedByInbound = true;
                otherNode.visualizationState.inboundDepth = curDepth + 1;
              } else {
                otherNode.visualizationState.selectedByOutbound = true;
                otherNode.visualizationState.outboundDepth = curDepth + 1;
              }
              nodeQueue.push(otherNode);
              seenNodes.add(otherNode.id);
              resultNodeSet.add(otherNode);
            }
            const edgeTraversedId = inboundTraversal ?
              getEdgeIdFromNodes(otherNode.id, curNode.id) :
              getEdgeIdFromNodes(curNode.id, otherNode.id);
            resultEdgeSet.add(this.getEdgeById(edgeTraversedId));
          }
        }
      }
    };

    const inboundDepth = filter.state.inboundDepth;
    const outboundDepth = filter.state.outboundDepth;

    updateResultBFS(/* inboundTraversal */ true, inboundSeenNodes,
        inboundNodeQueue, inboundDepth);
    updateResultBFS(/* inboundTraversal */ false, outboundSeenNodes,
        outboundNodeQueue, outboundDepth);

    // Special case: If inbound and outbound are both 0, both BFS will be a
    // no-op and the edges between filtered nodes will not be included. In this
    // case, we include those edges manually.
    if (inboundDepth === 0 && outboundDepth === 0) {
      for (const filterNode of resultNodeSet) {
        for (const otherNode of filterNode.inbound) {
          if (resultNodeSet.has(otherNode)) {
            resultEdgeSet.add(this.getEdgeById(
                getEdgeIdFromNodes(otherNode.id, filterNode.id)));
          }
        }
        for (const otherNode of filterNode.outbound) {
          if (resultNodeSet.has(otherNode)) {
            resultEdgeSet.add(this.getEdgeById(
                getEdgeIdFromNodes(filterNode.id, otherNode.id)));
          }
        }
      }
    }

    return {
      nodes: [...resultNodeSet],
      edges: [...resultEdgeSet],
    };
  }
}

export {
  Node,
  Edge,
  GraphModel,
  D3GraphData,
};
