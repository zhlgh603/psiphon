//
//  SearchField.h
//  TabbedBrowser
//
//  Created by eugene on 2013-03-07.
//  Copyright (c) 2013 Psiphon. All rights reserved.
//

#import <UIKit/UIKit.h>

typedef NS_ENUM(NSUInteger, SearchFieldState) {
    SearchFieldStateDisabled = 1,
    SearchFieldStateReload = 1<<2,
    SearchFieldStateStop = 1<<3
};

@interface AddressField : UITextField

- (id)initWithDelegate:(id<UITextFieldDelegate>)delegate;

@property (readonly, nonatomic) UIButton *reloadItem;
@property (readonly, nonatomic) UIButton *stopItem;

@property (assign, nonatomic) SearchFieldState state;

@end
