// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_password_check_item.h"

#import "ios/chrome/browser/ui/settings/cells/settings_password_check_cell.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SettingsPasswordCheckItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SettingsPasswordCheckCell class];
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(SettingsPasswordCheckCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  if (self.enabled) {
    [cell setIconImage:self.image withTintColor:self.tintColor];
    self.indicatorHidden ? [cell hideActivityIndicator]
                         : [cell showActivityIndicator];
    cell.textLabel.textColor = UIColor.cr_labelColor;
    cell.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
  } else {
    [cell setIconImage:nil withTintColor:nil];
    [cell hideActivityIndicator];
    cell.textLabel.textColor = UIColor.cr_secondaryLabelColor;
    cell.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
}

@end
