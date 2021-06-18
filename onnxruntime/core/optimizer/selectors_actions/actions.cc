// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "core/optimizer/selectors_actions/actions.h"
#include "core/optimizer/selectors_actions/helpers.h"
#include "core/optimizer/utils.h"

using namespace ONNX_NAMESPACE;
using namespace ::onnxruntime::common;
namespace onnxruntime {

namespace {

// Check if a node involved in an optimization can be safely removed.
// This requires that the optimizer correctly handles nodes producing graph outputs and does not attempt to delete
// one of those nodes unless it has created a new source for the graph output. As we can't easily remove the NodeArg
// from the Node::OutputDefs for the node being removed, we do not check if the node provides graph outputs here.
bool CanSafelyRemoveNode(Node& node_to_remove, const std::unordered_set<const Node*>& removal_set) {
  bool safe = true;
  for (auto iter = node_to_remove.OutputEdgesBegin(), end = node_to_remove.OutputEdgesEnd(); iter != end; ++iter) {
    if (removal_set.find(&iter->GetNode()) == removal_set.cend()) {
      safe = false;
      break;
    }
  }

  return safe;
}

// remove nodes that are safe to do so. 'safe' means no output edges to nodes not in the set of nodes being removed.
// there is NO check on a node producing a graph output. we assume the optimizer has handled that already.
void SafelyRemoveNodes(Graph& graph, const gsl::span<Node* const>& nodes_to_remove) {
  std::unordered_set<const Node*> removal_set(nodes_to_remove.cbegin(), nodes_to_remove.cend());

  for (Node* node : nodes_to_remove) {
    if (node && CanSafelyRemoveNode(*node, removal_set)) {
      // TODO: It's slightly insane we don't support optionally removing the output edges as part of Graph::RemoveNode
      // but to make that change we need to validate a lot of existing code
      graph_utils::RemoveNodeOutputEdges(graph, *node);
      graph.RemoveNode(node->Index());
    }
  }
}
}  // namespace

Status RemoveNodes::operator()(Graph& graph, const NodesToOptimize& selected_nodes) const {
  std::vector<Node*> nodes_to_remove;
  nodes_to_remove.reserve(selected_nodes.AllNodes().size());

  for (Node* node : selected_nodes.Inputs(nodes_to_remove_.input_node_indexes)) {
    nodes_to_remove.push_back(node);
  }

  if (nodes_to_remove_.include_target_node) {
    nodes_to_remove.push_back(selected_nodes.Target());
  }

  for (Node* node : selected_nodes.Outputs(nodes_to_remove_.output_node_indexes)) {
    nodes_to_remove.push_back(node);
  }

  SafelyRemoveNodes(graph, nodes_to_remove);

  return Status::OK();
}

Status RemoveAllNodes::operator()(Graph& graph, const NodesToOptimize& selected_nodes) const {
  // std::vector<Node*> nodes_to_remove(selected_nodes.AllNodes());
  SafelyRemoveNodes(graph, selected_nodes.AllNodes());

  return Status::OK();
}

Status MoveInputOutput::operator()(Graph& graph, const NodesToOptimize& selected_nodes) const {
  std::vector<Node*> src_nodes{nullptr};

  // these calls currently require the requested nodes to exist
  if (!move_info_.value_move_info.is_variadic) {
    // common case first. just set value in src_nodes (avoids cost of creating a new vector)
    src_nodes[0] = selected_nodes.GetNodeAtLocation(move_info_.src_node);
  } else {
    src_nodes = selected_nodes.GetNodesAtLocation(move_info_.src_node);
  }

  Node& dest_node = *selected_nodes.Target();

  for (Node* src_node : src_nodes) {
    if (src_node != nullptr) {
      ORT_RETURN_IF_ERROR(MoveInputOutputHelper::Move(graph, *src_node, dest_node, move_info_.value_move_info));
    }
  }

  return Status::OK();
}

Status MergeIntoExisting::operator()(Graph& graph, const NodesToOptimize& selected_nodes) const {
  // needs NodeAndMoveInfo
  for (const auto& value_mover : value_movers_) {
    // move inputs/output and associated edges
    ORT_RETURN_IF_ERROR(value_mover(graph, selected_nodes));
  }

  return node_remover_(graph, selected_nodes);
}

}  // namespace onnxruntime