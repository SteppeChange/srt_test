//
//  ViewController.m
//  SRTTest
//
//  Created by Alexey on 10/05/2019.
//  Copyright © 2019 Steppechange. All rights reserved.
//

#import "ViewController.h"
#import "SRTTestThread.h"
#include <arpa/inet.h>

@interface ViewController () <UITextFieldDelegate>

@property (nonatomic, strong) SRTTestThread *workingThread;

@property (nonatomic, weak) IBOutlet UIButton *serverButton;

@property (nonatomic, weak) IBOutlet UITextField *serverAddressTextField;
@property (nonatomic, weak) IBOutlet UIButton *clientButton;

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    [self configureClientButton:self.serverAddressTextField.text];
}

- (IBAction)onStartServer:(id)sender {
    self.serverButton.enabled = NO;
    self.clientButton.enabled = NO;
    self.serverAddressTextField.enabled = NO;
    
    if (self.workingThread) {
        return;
    }
    
    self.workingThread = [[SRTTestThread alloc] init];
    self.workingThread.isClient = NO;
    [self.workingThread start];
}

- (IBAction)onStartClient:(id)sender {
    self.serverButton.enabled = NO;
    self.clientButton.enabled = NO;
    self.serverAddressTextField.enabled = NO;

    if (self.workingThread) {
        return;
    }
    
    self.workingThread = [[SRTTestThread alloc] init];
    self.workingThread.isClient = YES;
    self.workingThread.serverAddress = self.serverAddressTextField.text;
    [self.workingThread start];
}

- (BOOL) isValidIPAddress:(NSString *) address {
    const char *utf8 = [address UTF8String];
    int success;
    
    struct in_addr dst;
    success = inet_pton(AF_INET, utf8, &dst);
    if (success != 1) {
        struct in6_addr dst6;
        success = inet_pton(AF_INET6, utf8, &dst6);
    }
    
    return success == 1;
}

- (void) configureClientButton:(NSString *) serverAddress {
    if (serverAddress.length == 0) {
        self.clientButton.enabled = NO;
        return;
    }
    
    NSArray<NSString *> *comps = [serverAddress componentsSeparatedByString:@":"];
    if (comps.count != 2) {
        self.clientButton.enabled = NO;
        return;
    }
    
    if (comps[0].length == 0 || ![self isValidIPAddress:comps[0]]) {
        self.clientButton.enabled = NO;
        return;
    }
    
    if ([comps[1] integerValue] <= 0) {
        self.clientButton.enabled = NO;
        return;
    }

    self.clientButton.enabled = YES;
}

- (BOOL) textField:(UITextField *)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string {
    NSString *serverAddress = [textField.text stringByReplacingCharactersInRange:range withString:string];
    
    [self configureClientButton:serverAddress];
    
    return YES;
}

- (BOOL) textFieldShouldReturn:(UITextField *)textField {
    [textField resignFirstResponder];
    return YES;
}


@end
