/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 * Copyright (C) 2010 Daniel Bates (dbates@intudata.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/layout/layout_list_marker.h"

#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/list_marker.h"
#include "third_party/blink/renderer/core/layout/list_marker_text.h"
#include "third_party/blink/renderer/core/paint/list_marker_painter.h"
#include "third_party/blink/renderer/platform/fonts/font.h"

namespace blink {

LayoutListMarker::LayoutListMarker(Element* element) : LayoutBox(element) {
  DCHECK(ListItem());
  SetInline(true);
  SetIsAtomicInlineLevel(true);
}

LayoutListMarker::~LayoutListMarker() = default;

void LayoutListMarker::WillBeDestroyed() {
  if (image_)
    image_->RemoveClient(this);
  LayoutBox::WillBeDestroyed();
}

const LayoutListItem* LayoutListMarker::ListItem() const {
  LayoutObject* list_item = GetNode()->parentNode()->GetLayoutObject();
  DCHECK(list_item);
  DCHECK(list_item->IsListItem());
  return ToLayoutListItem(list_item);
}

LayoutSize LayoutListMarker::ImageBulletSize() const {
  DCHECK(IsImage());
  const SimpleFontData* font_data = StyleRef().GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return LayoutSize();

  // FIXME: This is a somewhat arbitrary default width. Generated images for
  // markers really won't become particularly useful until we support the CSS3
  // marker pseudoclass to allow control over the width and height of the
  // marker box.
  LayoutUnit bullet_width =
      font_data->GetFontMetrics().Ascent() / LayoutUnit(2);
  return RoundedLayoutSize(
      image_->ImageSize(GetDocument(), StyleRef().EffectiveZoom(),
                        LayoutSize(bullet_width, bullet_width),
                        LayoutObject::ShouldRespectImageOrientation(this)));
}

void LayoutListMarker::StyleWillChange(StyleDifference diff,
                                       const ComputedStyle& new_style) {
  if (Style() &&
      (new_style.ListStylePosition() != StyleRef().ListStylePosition() ||
       new_style.ListStyleType() != StyleRef().ListStyleType() ||
       (new_style.ListStyleType() == EListStyleType::kString &&
        new_style.ListStyleStringValue() !=
            StyleRef().ListStyleStringValue()))) {
    SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kStyleChange);
  }

  LayoutBox::StyleWillChange(diff, new_style);
}

void LayoutListMarker::StyleDidChange(StyleDifference diff,
                                      const ComputedStyle* old_style) {
  LayoutBox::StyleDidChange(diff, old_style);

  if (image_ != StyleRef().ListStyleImage()) {
    if (image_)
      image_->RemoveClient(this);
    image_ = StyleRef().ListStyleImage();
    if (image_)
      image_->AddClient(this);
  }
}

InlineBox* LayoutListMarker::CreateInlineBox() {
  InlineBox* result = LayoutBox::CreateInlineBox();
  result->SetIsText(IsText());
  return result;
}

bool LayoutListMarker::IsImage() const {
  return image_ && !image_->ErrorOccurred();
}

void LayoutListMarker::Paint(const PaintInfo& paint_info) const {
  ListMarkerPainter(*this).Paint(paint_info);
}

void LayoutListMarker::UpdateLayout() {
  DCHECK(NeedsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  LayoutUnit block_offset = LogicalTop();
  const LayoutListItem* list_item = ListItem();
  for (LayoutBox* o = ParentBox(); o && o != list_item; o = o->ParentBox()) {
    block_offset += o->LogicalTop();
  }
  if (list_item->StyleRef().IsLeftToRightDirection()) {
    line_offset_ = list_item->LogicalLeftOffsetForLine(
        block_offset, kDoNotIndentText, LayoutUnit());
  } else {
    line_offset_ = list_item->LogicalRightOffsetForLine(
        block_offset, kDoNotIndentText, LayoutUnit());
  }
  if (IsImage()) {
    UpdateMarginsAndContent();
    LayoutSize image_size(ImageBulletSize());
    SetWidth(image_size.Width());
    SetHeight(image_size.Height());
  } else {
    const SimpleFontData* font_data = StyleRef().GetFont().PrimaryFont();
    DCHECK(font_data);
    SetLogicalWidth(PreferredLogicalWidths().min_size);
    SetLogicalHeight(
        LayoutUnit(font_data ? font_data->GetFontMetrics().Height() : 0));
  }

  ClearNeedsLayout();
}

void LayoutListMarker::ImageChanged(WrappedImagePtr o, CanDeferInvalidation) {
  // A list marker can't have a background or border image, so no need to call
  // the base class method.
  if (!image_ || o != image_->Data())
    return;

  LayoutSize image_size = IsImage() ? ImageBulletSize() : LayoutSize();
  if (Size() != image_size || image_->ErrorOccurred()) {
    SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kImageChanged);
  } else {
    SetShouldDoFullPaintInvalidation();
  }
}

void LayoutListMarker::UpdateMarginsAndContent() {
  UpdateMargins(PreferredLogicalWidths().min_size);
}

void LayoutListMarker::UpdateContent() {
  DCHECK(IntrinsicLogicalWidthsDirty());

  text_ = "";

  if (IsImage())
    return;

  switch (GetListStyleCategory()) {
    case ListMarker::ListStyleCategory::kNone:
      break;
    case ListMarker::ListStyleCategory::kSymbol:
      text_ = list_marker_text::GetText(StyleRef().ListStyleType(),
                                        0);  // value is ignored for these types
      break;
    case ListMarker::ListStyleCategory::kLanguage:
      text_ = list_marker_text::GetText(StyleRef().ListStyleType(),
                                        ListItem()->Value());
      break;
    case ListMarker::ListStyleCategory::kStaticString:
      text_ = StyleRef().ListStyleStringValue();
      break;
  }
}

String LayoutListMarker::TextAlternative() const {
  if (GetListStyleCategory() == ListMarker::ListStyleCategory::kStaticString)
    return text_;
  UChar suffix =
      list_marker_text::Suffix(StyleRef().ListStyleType(), ListItem()->Value());
  // Return suffix after the marker text, even in RTL, reflecting speech order.
  return text_ + suffix + ' ';
}

LayoutUnit LayoutListMarker::GetWidthOfText(
    ListMarker::ListStyleCategory category) const {
  // TODO(crbug.com/1012289): this code doesn't support bidi algorithm.
  if (text_.IsEmpty())
    return LayoutUnit();
  const Font& font = StyleRef().GetFont();
  LayoutUnit item_width = LayoutUnit(font.Width(TextRun(text_)));
  if (category == ListMarker::ListStyleCategory::kStaticString) {
    // Don't add a suffix.
    return item_width;
  }
  // TODO(wkorman): Look into constructing a text run for both text and suffix
  // and painting them together.
  UChar suffix[2] = {
      list_marker_text::Suffix(StyleRef().ListStyleType(), ListItem()->Value()),
      ' '};
  TextRun run =
      ConstructTextRun(font, suffix, 2, StyleRef(), StyleRef().Direction());
  LayoutUnit suffix_space_width = LayoutUnit(font.Width(run));
  return item_width + suffix_space_width;
}

MinMaxSizes LayoutListMarker::ComputeIntrinsicLogicalWidths() const {
  DCHECK(IntrinsicLogicalWidthsDirty());
  const_cast<LayoutListMarker*>(this)->UpdateContent();

  MinMaxSizes sizes;
  if (IsImage()) {
    LayoutSize image_size(ImageBulletSize());
    sizes = StyleRef().IsHorizontalWritingMode() ? image_size.Width()
                                                 : image_size.Height();
  } else {
    ListMarker::ListStyleCategory category = GetListStyleCategory();
    switch (category) {
      case ListMarker::ListStyleCategory::kNone:
        break;
      case ListMarker::ListStyleCategory::kSymbol:
        sizes = ListMarker::WidthOfSymbol(StyleRef());
        break;
      case ListMarker::ListStyleCategory::kLanguage:
      case ListMarker::ListStyleCategory::kStaticString:
        sizes = GetWidthOfText(category);
        break;
    }
  }

  const_cast<LayoutListMarker*>(this)->UpdateMargins(sizes.min_size);
  return sizes;
}

MinMaxSizes LayoutListMarker::PreferredLogicalWidths() const {
  return IntrinsicLogicalWidths();
}

void LayoutListMarker::UpdateMargins(LayoutUnit marker_inline_size) {
  LayoutUnit margin_start;
  LayoutUnit margin_end;
  const ComputedStyle& style = StyleRef();
  if (IsInside()) {
    std::tie(margin_start, margin_end) =
        ListMarker::InlineMarginsForInside(style, IsImage());
  } else {
    std::tie(margin_start, margin_end) = ListMarker::InlineMarginsForOutside(
        style, IsImage(), marker_inline_size);
  }

  SetMarginStart(margin_start);
  SetMarginEnd(margin_end);
}

LayoutUnit LayoutListMarker::LineHeight(
    bool first_line,
    LineDirectionMode direction,
    LinePositionMode line_position_mode) const {
  if (!IsImage())
    return ListItem()->LineHeight(first_line, direction,
                                  kPositionOfInteriorLineBoxes);
  return LayoutBox::LineHeight(first_line, direction, line_position_mode);
}

LayoutUnit LayoutListMarker::BaselinePosition(
    FontBaseline baseline_type,
    bool first_line,
    LineDirectionMode direction,
    LinePositionMode line_position_mode) const {
  DCHECK_EQ(line_position_mode, kPositionOnContainingLine);
  if (!IsImage())
    return ListItem()->BaselinePosition(baseline_type, first_line, direction,
                                        kPositionOfInteriorLineBoxes);
  return LayoutBox::BaselinePosition(baseline_type, first_line, direction,
                                     line_position_mode);
}

ListMarker::ListStyleCategory LayoutListMarker::GetListStyleCategory() const {
  return ListMarker::GetListStyleCategory(StyleRef().ListStyleType());
}

bool LayoutListMarker::IsInside() const {
  const LayoutListItem* list_item = ListItem();
  const ComputedStyle& parent_style = list_item->StyleRef();
  return parent_style.ListStylePosition() == EListStylePosition::kInside ||
         (IsA<HTMLLIElement>(list_item->GetNode()) &&
          !parent_style.IsInsideListElement());
}

LayoutRect LayoutListMarker::GetRelativeMarkerRect() const {
  if (IsImage())
    return LayoutRect(LayoutPoint(), ImageBulletSize());

  LayoutRect relative_rect;
  ListMarker::ListStyleCategory category = GetListStyleCategory();
  switch (category) {
    case ListMarker::ListStyleCategory::kNone:
      return LayoutRect();
    case ListMarker::ListStyleCategory::kSymbol:
      return ListMarker::RelativeSymbolMarkerRect(StyleRef(), Size().Width());
    case ListMarker::ListStyleCategory::kLanguage:
    case ListMarker::ListStyleCategory::kStaticString: {
      const SimpleFontData* font_data = StyleRef().GetFont().PrimaryFont();
      DCHECK(font_data);
      if (!font_data)
        return relative_rect;
      relative_rect =
          LayoutRect(LayoutUnit(), LayoutUnit(), GetWidthOfText(category),
                     LayoutUnit(font_data->GetFontMetrics().Height()));
      break;
    }
  }

  if (!StyleRef().IsHorizontalWritingMode()) {
    relative_rect = relative_rect.TransposedRect();
    relative_rect.SetX(Size().Width() - relative_rect.X() -
                       relative_rect.Width());
  }
  return relative_rect;
}

}  // namespace blink
