// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/selector.h"

#include "base/macros.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::UnorderedElementsAre;

namespace autofill_assistant {
namespace {

TEST(SelectorTest, Constructor_Simple) {
  Selector selector({"#test"});
  ASSERT_EQ(1, selector.proto.filters().size());
  EXPECT_EQ("#test", selector.proto.filters(0).css_selector());
}

TEST(SelectorTest, Constructor_WithIframe) {
  Selector selector({"#frame", "#test"});
  ASSERT_EQ(4, selector.proto.filters().size());
  EXPECT_EQ("#frame", selector.proto.filters(0).css_selector());
  EXPECT_EQ(selector.proto.filters(1).filter_case(),
            SelectorProto::Filter::kPickOne);
  EXPECT_EQ(selector.proto.filters(2).filter_case(),
            SelectorProto::Filter::kEnterFrame);
  EXPECT_EQ("#test", selector.proto.filters(3).css_selector());
}

TEST(SelectorTest, FromProto) {
  SelectorProto proto;
  proto.add_filters()->set_css_selector("#test");

  EXPECT_EQ(Selector({"#test"}), Selector(proto));
}

TEST(SelectorTest, Comparison) {
  // Note that comparison tests cover < indirectly through ==, since a == b is
  // defined as !(a < b) && !(b < a). This makes sense, as what matters is that
  // there is an order but what the order is doesn't matter.

  EXPECT_FALSE(Selector({"a"}) == Selector({"b"}));
  EXPECT_TRUE(Selector({"a"}) == Selector({"a"}));
}

TEST(SelectorTest, SelectorInSet) {
  std::set<Selector> selectors;
  selectors.insert(Selector({"a"}));
  selectors.insert(Selector({"a"}));
  selectors.insert(Selector({"b"}));
  selectors.insert(Selector({"c"}));
  EXPECT_THAT(selectors, UnorderedElementsAre(Selector({"a"}), Selector({"b"}),
                                              Selector({"c"})));
}

TEST(SelectorTest, Comparison_PseudoType) {
  EXPECT_FALSE(Selector({"a"}).SetPseudoType(PseudoType::BEFORE) ==
               Selector({"a"}).SetPseudoType(PseudoType::AFTER));
  EXPECT_FALSE(Selector({"a"}).SetPseudoType(PseudoType::BEFORE) ==
               Selector({"a"}).SetPseudoType(PseudoType::AFTER));
  EXPECT_FALSE(Selector({"b"}) ==
               Selector({"a"}).SetPseudoType(PseudoType::BEFORE));
  EXPECT_FALSE(Selector({"b"}) ==
               Selector({"a"}).SetPseudoType(PseudoType::BEFORE));
  EXPECT_TRUE(Selector({"a"}).SetPseudoType(PseudoType::BEFORE) ==
              Selector({"a"}).SetPseudoType(PseudoType::BEFORE));
}

TEST(SelectorTest, Comparison_Visibility) {
  EXPECT_FALSE(Selector({"a"}) == Selector({"a"}).MustBeVisible());
  EXPECT_TRUE(Selector({"a"}).MustBeVisible() ==
              Selector({"a"}).MustBeVisible());
}

TEST(SelectorTest, Comparison_InnerText) {
  EXPECT_FALSE(Selector({"a"}).MatchingInnerText("a") ==
               Selector({"a"}).MatchingInnerText("b"));
  EXPECT_TRUE(Selector({"a"}).MatchingInnerText("a") ==
              Selector({"a"}).MatchingInnerText("a"));

  EXPECT_FALSE(Selector({"a"}).MatchingInnerText("a", false) ==
               Selector({"a"}).MatchingInnerText("a", true));
  EXPECT_TRUE(Selector({"a"}).MatchingInnerText("a", true) ==
              Selector({"a"}).MatchingInnerText("a", true));
}

TEST(SelectorTest, Comparison_Value) {
  EXPECT_FALSE(Selector({"a"}).MatchingValue("a") ==
               Selector({"a"}).MatchingValue("b"));
  EXPECT_TRUE(Selector({"a"}).MatchingValue("a") ==
              Selector({"a"}).MatchingValue("a"));

  EXPECT_FALSE(Selector({"a"}).MatchingValue("a", false) ==
               Selector({"a"}).MatchingValue("a", true));
  EXPECT_TRUE(Selector({"a"}).MatchingValue("a", true) ==
              Selector({"a"}).MatchingValue("a", true));
}

TEST(SelectorTest, Comparison_Proximity) {
  SelectorProto selector;
  selector.add_filters()->set_css_selector("button");
  auto* closest_to_button = selector.add_filters()->mutable_closest();
  closest_to_button->mutable_target()->Add()->set_css_selector("#label1");

  EXPECT_TRUE(Selector(selector) == Selector(selector));

  // Different relative positions
  SelectorProto left = selector;
  left.mutable_filters(0)->mutable_closest()->set_relative_position(
      SelectorProto::ProximityFilter::LEFT);

  SelectorProto right = selector;
  right.mutable_filters(0)->mutable_closest()->set_relative_position(
      SelectorProto::ProximityFilter::RIGHT);

  EXPECT_TRUE(Selector(right) == Selector(right));
  EXPECT_TRUE(Selector(left) == Selector(left));
  EXPECT_FALSE(Selector(left) == Selector(right));

  // Different alignment
  SelectorProto aligned = selector;
  selector.mutable_filters(0)->mutable_closest()->set_in_alignment(true);
  EXPECT_TRUE(Selector(aligned) == Selector(aligned));
  EXPECT_FALSE(Selector(selector) == Selector(aligned));

  // Different targets
  SelectorProto label2 = selector;
  label2.mutable_filters(0)
      ->mutable_closest()
      ->mutable_target()
      ->Add()
      ->set_css_selector("#label2");

  EXPECT_TRUE(Selector(label2) == Selector(label2));
  EXPECT_FALSE(Selector(selector) == Selector(label2));
}

TEST(SelectorTest, Comparison_Frames) {
  Selector ab({"a", "b"});
  EXPECT_EQ(ab, ab);

  Selector cb({"c", "b"});
  EXPECT_TRUE(cb == cb);
  EXPECT_FALSE(ab == cb);

  Selector b({"b"});
  EXPECT_TRUE(b == b);
  EXPECT_FALSE(ab == b);
}

TEST(SelectorTest, Comparison_MultipleFilters) {
  Selector abcdef;
  abcdef.proto.add_filters()->set_css_selector("abc");
  abcdef.proto.add_filters()->set_css_selector("def");

  Selector abcdef2;
  abcdef2.proto.add_filters()->set_css_selector("abc");
  abcdef2.proto.add_filters()->set_css_selector("def");
  EXPECT_EQ(abcdef, abcdef2);

  Selector defabc;
  defabc.proto.add_filters()->set_css_selector("def");
  defabc.proto.add_filters()->set_css_selector("abc");
  EXPECT_TRUE(defabc == defabc);
  EXPECT_FALSE(abcdef == defabc);

  Selector abc;
  abc.proto.add_filters()->set_css_selector("abc");
  EXPECT_TRUE(abc == abc);
  EXPECT_FALSE(abcdef == abc);
}

}  // namespace
}  // namespace autofill_assistant
