// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_DRAG_DROP_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_DRAG_DROP_HANDLER_H_

#import <UIKit/UIKit.h>

// A protocol for objects that handle drag and drop interactions for a grid
// involving the model layer.
@protocol GridDragDropHandler

// Returns a drag item encapsulating all necessary information to perform
// valid drop operations. Note that this drag item may be dropped anywhere,
// including within the same collection, another view, or other apps. |itemID|
// uniquely identifies a single item in the model layer known to the object
// conforming to this protocol.
- (UIDragItem*)dragItemForItemWithID:(NSString*)itemID;

// Returns a value which represents how a drag activity should be resolved when
// the user drops a drag item. |session| contains pertinent information
// including the drag item.
- (UIDropOperation)dropOperationForDropSession:(id<UIDropSession>)session;

// Tells the receiver to incorporate the |dragItem| into the model layer at the
// |destinationIndex|. |fromSameCollection| is an indication that the operation
// is a reorder within the same collection.
- (void)dropItem:(UIDragItem*)dragItem
               toIndex:(NSUInteger)destinationIndex
    fromSameCollection:(BOOL)fromSameCollection;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_DRAG_DROP_HANDLER_H_
