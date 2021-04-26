// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_base.h"

#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/test_browser_accessibility_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace content {

class AccessibilityTreeFormatterBaseTest : public testing::Test {
 public:
  AccessibilityTreeFormatterBaseTest() = default;
  ~AccessibilityTreeFormatterBaseTest() override = default;

 protected:
  std::unique_ptr<TestBrowserAccessibilityDelegate>
      test_browser_accessibility_delegate_;

 private:
  void SetUp() override {
    test_browser_accessibility_delegate_ =
        std::make_unique<TestBrowserAccessibilityDelegate>();
  }

  DISALLOW_COPY_AND_ASSIGN(AccessibilityTreeFormatterBaseTest);
};

PropertyNode Parse(const char* input) {
  AccessibilityTreeFormatter::PropertyFilter filter(
      base::UTF8ToUTF16(input),
      AccessibilityTreeFormatter::PropertyFilter::ALLOW);
  return PropertyNode::FromPropertyFilter(filter);
}

PropertyNode GetArgumentNode(const char* input) {
  auto got = Parse(input);
  if (got.parameters.size() == 0) {
    return PropertyNode();
  }
  return std::move(got.parameters[0]);
}

void ParseAndCheck(const char* input, const char* expected) {
  auto got = Parse(input).ToString();
  EXPECT_EQ(got, expected);
}

TEST_F(AccessibilityTreeFormatterBaseTest, ParseProperty) {
  // Properties and methods.
  ParseAndCheck("Role", "Role");
  ParseAndCheck("ChildAt(3)", "ChildAt(3)");
  ParseAndCheck("Cell(3, 4)", "Cell(3, 4)");
  ParseAndCheck("Volume(3, 4, 5)", "Volume(3, 4, 5)");
  ParseAndCheck("TableFor(CellBy(id))", "TableFor(CellBy(id))");
  ParseAndCheck("A(B(1), 2)", "A(B(1), 2)");
  ParseAndCheck("A(B(1), 2, C(3, 4))", "A(B(1), 2, C(3, 4))");
  ParseAndCheck("[3, 4]", "[](3, 4)");
  ParseAndCheck("Cell([3, 4])", "Cell([](3, 4))");

  // Arguments
  ParseAndCheck("Text({val: 1})", "Text({}(val: 1))");
  ParseAndCheck("Text({lat: 1, len: 1})", "Text({}(lat: 1, len: 1))");
  ParseAndCheck("Text({dict: {val: 1}})", "Text({}(dict: {}(val: 1)))");
  ParseAndCheck("Text({dict: {val: 1}, 3})", "Text({}(dict: {}(val: 1), 3))");
  ParseAndCheck("Text({dict: [1, 2]})", "Text({}(dict: [](1, 2)))");
  ParseAndCheck("Text({dict: ValueFor(1)})", "Text({}(dict: ValueFor(1)))");

  // Line indexes filter.
  ParseAndCheck(":3,:5;AXDOMClassList", ":3,:5;AXDOMClassList");

  // Wrong format.
  ParseAndCheck("Role(3", "Role(3)");
  ParseAndCheck("TableFor(CellBy(id", "TableFor(CellBy(id))");
  ParseAndCheck("[3, 4", "[](3, 4)");

  // Arguments conversion
  EXPECT_EQ(GetArgumentNode("ChildAt([3])").IsArray(), true);
  EXPECT_EQ(GetArgumentNode("Text({loc: 3, len: 2})").IsDict(), true);
  EXPECT_EQ(GetArgumentNode("ChildAt(3)").IsDict(), false);
  EXPECT_EQ(GetArgumentNode("ChildAt(3)").IsArray(), false);
  EXPECT_EQ(GetArgumentNode("ChildAt(3)").AsInt(), 3);
  EXPECT_EQ(GetArgumentNode("Text({start: :1, dir: forward})").FindKey("start"),
            base::ASCIIToUTF16(":1"));
  EXPECT_EQ(GetArgumentNode("Text({start: :1, dir: forward})").FindKey("dir"),
            base::ASCIIToUTF16("forward"));
  EXPECT_EQ(
      GetArgumentNode("Text({start: :1, dir: forward})").FindKey("notexists"),
      base::nullopt);
  EXPECT_EQ(GetArgumentNode("Text({loc: 3, len: 2})").FindIntKey("loc"), 3);
  EXPECT_EQ(GetArgumentNode("Text({loc: 3, len: 2})").FindIntKey("len"), 2);
  EXPECT_EQ(GetArgumentNode("Text({loc: 3, len: 2})").FindIntKey("notexists"),
            base::nullopt);
}

}  // namespace content
