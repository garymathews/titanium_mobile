/**
 * Appcelerator Titanium Mobile
 * Copyright (c) 2009-2018 by Appcelerator, Inc. All Rights Reserved.
 * Licensed under the terms of the Apache Public License
 * Please see the LICENSE included with this distribution for details.
 * 
 * WARNING: This is generated code. Modify at your own risk and without support.
 */

#import "TiErrorController.h"
#import "TiApp.h"
#import "TiBase.h"
#import "TiUtils.h"
#import <QuartzCore/QuartzCore.h>

@implementation TiErrorController

- (id)initWithError:(NSString *)error_
{
  if (self = [super init]) {
    error = [error_ retain];
  }
  return self;
}

- (void)dealloc
{
  RELEASE_TO_NIL(scrollView);
  RELEASE_TO_NIL(messageView);
  RELEASE_TO_NIL(continueButton);
  [super dealloc];
}

- (void)dismiss:(id)sender
{
  [[TiApp app] hideModalController:self animated:YES];
}

- (void)loadView
{
  [super loadView];
  self.modalTransitionStyle = UIModalTransitionStyleCoverVertical;
  [self.view setBackgroundColor:[UIColor colorWithRed:0.96 green:0.96 blue:0.96 alpha:1.0]];

  // release previous allocations
  RELEASE_TO_NIL(scrollView);
  RELEASE_TO_NIL(messageView);
  RELEASE_TO_NIL(continueButton);

  // create scrollable view for message
  scrollView = [[UIScrollView alloc] init];
  [scrollView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [scrollView setContentMode:UIViewContentModeScaleToFill];
  [scrollView setBackgroundColor:[UIColor clearColor]];
  [self.view addSubview:scrollView];

  // constraints to fill parent
  [self.view addConstraints:@[
    [NSLayoutConstraint constraintWithItem:scrollView
                                 attribute:NSLayoutAttributeWidth
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self.view
                                 attribute:NSLayoutAttributeWidth
                                multiplier:1
                                  constant:0],
    [NSLayoutConstraint constraintWithItem:scrollView
                                 attribute:NSLayoutAttributeHeight
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self.view
                                 attribute:NSLayoutAttributeHeight
                                multiplier:1
                                  constant:0]
  ]];

  // create message view inside scrollable view
  messageView = [[UITextView alloc] init];
  [messageView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [messageView setContentMode:UIViewContentModeScaleToFill];
  [messageView setBounces:false];
  [messageView setBouncesZoom:false];
  [messageView setEditable:false];
  [messageView setScrollEnabled:false];
  [messageView setMultipleTouchEnabled:true];
  [messageView setBackgroundColor:[UIColor clearColor]];
  [messageView setTextColor:[UIColor colorWithRed:0.90 green:0.22 blue:0.21 alpha:1.0]];
  [messageView setText:error];
  [messageView setFont:[UIFont fontWithName:@"Courier" size:18]];
  [scrollView addSubview:messageView];

  // constraints for top and left padding
  [scrollView addConstraints:@[
    [NSLayoutConstraint constraintWithItem:messageView
                                 attribute:NSLayoutAttributeTop
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:scrollView
                                 attribute:NSLayoutAttributeTop
                                multiplier:1
                                  constant:16],
    [NSLayoutConstraint constraintWithItem:messageView
                                 attribute:NSLayoutAttributeLeft
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:scrollView
                                 attribute:NSLayoutAttributeLeft
                                multiplier:1
                                  constant:8],
  ]];

  // create continue button to dismiss exception
  continueButton = [UIButton buttonWithType:UIButtonTypeRoundedRect];
  [continueButton setTranslatesAutoresizingMaskIntoConstraints:NO];
  [continueButton setBackgroundColor:[UIColor colorWithRed:0.90 green:0.22 blue:0.21 alpha:1.0]];

  // set title and adjust font attributes
  NSMutableAttributedString *continueAttributes = [[NSMutableAttributedString alloc] initWithString:@"CONTINUE"];
  [continueAttributes addAttribute:NSForegroundColorAttributeName value:[UIColor whiteColor] range:NSMakeRange(0, [continueAttributes length])];
  [continueAttributes addAttribute:NSFontAttributeName value:[UIFont boldSystemFontOfSize:18] range:NSMakeRange(0, [continueAttributes length])];
  [continueButton setAttributedTitle:continueAttributes forState:UIControlStateNormal];

  // define button behaviour
  [continueButton addTarget:self action:@selector(dismiss:) forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:continueButton];

  // constrain button to bottom of view
  [self.view addConstraints:@[
    [NSLayoutConstraint constraintWithItem:continueButton
                                 attribute:NSLayoutAttributeWidth
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self.view
                                 attribute:NSLayoutAttributeWidth
                                multiplier:1
                                  constant:0],
    [NSLayoutConstraint constraintWithItem:continueButton
                                 attribute:NSLayoutAttributeHeight
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self.view
                                 attribute:NSLayoutAttributeHeight
                                multiplier:0.08
                                  constant:0],
    [NSLayoutConstraint constraintWithItem:continueButton
                                 attribute:NSLayoutAttributeBottom
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self.view
                                 attribute:NSLayoutAttributeBottom
                                multiplier:1
                                  constant:0]
  ]];

  // re-size message view scrolling content
  [messageView setContentSize:[messageView sizeThatFits:CGSizeMake(FLT_MAX, FLT_MAX)]];
  [messageView setFrame:CGRectMake(messageView.frame.origin.x, messageView.frame.origin.y, messageView.contentSize.width, messageView.contentSize.height)];
  [scrollView setContentSize:CGSizeMake(messageView.contentSize.width, messageView.contentSize.height)];

  [self.view layoutIfNeeded];
}

- (void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
  [self.view layoutIfNeeded];
}

@end
