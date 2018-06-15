// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPIWebkitBorderBefore.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSProperty.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

bool CSSShorthandPropertyAPIWebkitBorderBefore::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSProperty, 256>& properties) const {
  return CSSPropertyParserHelpers::ConsumeShorthandGreedilyViaLonghandAPIs(
      webkitBorderBeforeShorthand(), important, context, range, properties);
}

const CSSPropertyAPI&
CSSShorthandPropertyAPIWebkitBorderBefore::ResolveDirectionAwareProperty(
    TextDirection direction,
    WritingMode writing_mode) const {
  return ResolveToPhysicalPropertyAPI(direction, writing_mode, kBeforeSide,
                                      CSSPropertyAPI::BorderDirections());
}
}  // namespace blink
