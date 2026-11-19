#pragma once
#ifndef TREEPRINT_H
#define TREEPRINT_H

#include <algorithm>
#include <iostream>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>

// terminal view
template <typename NodeT>
void printSubtree(const NodeT *n, std::ostream &out, const std::string &prefix,
                  const std::string &label, bool last) {
  out << prefix << (last ? "└── " : "├── ") << label
      << (n ? n->getKey() : std::string("·")) << '\n';
  if (!n || (!n->left && !n->right))
    return;
  std::string childPrefix = prefix + (last ? "    " : "│   ");
  printSubtree(n->left.get(), out, childPrefix, "L: ", false);
  printSubtree(n->right.get(), out, childPrefix, "R: ", true);
}

template <typename NodeT> void PrintTree(const NodeT *root, std::ostream &out) {
  if (!root) {
    out << "(empty tree)\n";
    return;
  }
  out << root->getKey() << '\n';
  if (!root->left && !root->right)
    return;
  printSubtree(root->left.get(), out, "", "L: ", false);
  printSubtree(root->right.get(), out, "", "R: ", true);
}

// Graphviz DOT export
template <typename NodeT>
void dotEdges(const NodeT *n, std::ostream &out, int &nullId) {
  bool oneChild = (n->left == nullptr) != (n->right == nullptr);
  for (const NodeT *c : {n->left.get(), n->right.get()}) {
    if (c) {
      out << "  \"" << n->getKey() << "\" -> \"" << c->getKey() << "\";\n";
      dotEdges(c, out, nullId);
    } else if (oneChild) {
      // invisible placeholder so a lone child is drawn on the correct side
      out << "  null" << nullId << " [shape=point, style=invis];\n";
      out << "  \"" << n->getKey() << "\" -> null" << nullId
          << " [style=invis];\n";
      ++nullId;
    }
  }
}

template <typename NodeT> void WriteDot(const NodeT *root, std::ostream &out) {
  out << "digraph Tree {\n";
  out << "  node [shape=box, style=rounded, fontname=\"Helvetica\"];\n";
  if (root) {
    out << "  \"" << root->getKey() << "\";\n";
    int nullId = 0;
    dotEdges(root, out, nullId);
  }
  out << "}\n";
}

template <typename NodeT>
void svgLayout(const NodeT *n, int depth, int &nextX,
               std::unordered_map<const NodeT *, std::pair<int, int>> &pos) {
  if (!n)
    return;
  svgLayout(n->left.get(), depth + 1, nextX, pos);
  pos[n] = {nextX++, depth}; // in-order column, depth row
  svgLayout(n->right.get(), depth + 1, nextX, pos);
}

template <typename NodeT> void WriteSvg(const NodeT *root, std::ostream &out) {
  const int colW = 100, rowH = 80, boxW = 90, boxH = 30, margin = 20;

  std::unordered_map<const NodeT *, std::pair<int, int>> pos;
  int count = 0;
  svgLayout(root, 0, count, pos);

  int maxDepth = 0;
  for (auto &[n, p] : pos)
    maxDepth = std::max(maxDepth, p.second);

  int width = count * colW + 2 * margin;
  int height = (maxDepth + 1) * rowH + 2 * margin;

  // top-center point of a node's box, in pixels
  auto topCenter = [&](const NodeT *n) {
    auto [x, y] = pos.at(n);
    return std::pair<int, int>{margin + x * colW + colW / 2, margin + y * rowH};
  };

  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width
      << "\" height=\"" << height
      << "\" font-family=\"Helvetica, Arial, sans-serif\" font-size=\"14\">\n";
  out << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";

  // edges first
  for (auto &[n, p] : pos) {
    auto [x1, y1] = topCenter(n);
    for (const NodeT *c : {n->left.get(), n->right.get()}) {
      if (!c)
        continue;
      auto [x2, y2] = topCenter(c);
      out << "<line x1=\"" << x1 << "\" y1=\"" << y1 + boxH << "\" x2=\"" << x2
          << "\" y2=\"" << y2 << "\" stroke=\"#555\"/>\n";
    }
  }

  // nodes on top
  for (auto &[n, p] : pos) {
    auto [cx, top] = topCenter(n);
    out << "<rect x=\"" << cx - boxW / 2 << "\" y=\"" << top << "\" width=\""
        << boxW << "\" height=\"" << boxH
        << "\" rx=\"8\" fill=\"#eef3fb\" stroke=\"#333\"/>\n";
    out << "<text x=\"" << cx << "\" y=\"" << top + boxH / 2
        << "\" text-anchor=\"middle\" dominant-baseline=\"central\">"
        << n->getKey() << "</text>\n";
  }
  out << "</svg>\n";
}

#endif // !TREEPRINT_H
