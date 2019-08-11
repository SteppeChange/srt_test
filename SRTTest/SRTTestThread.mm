//
//  SRTTestThread.m
//  SRTTest
//
//  Created by Alexey on 10/05/2019.
//  Copyright © 2019 Steppechange. All rights reserved.
//

#import "SRTTestThread.h"

#include "srt_test.h"

static void antLogFunction(char const *text) {
    NSLog(@"%s", text);
}

@implementation SRTTestThread

- (void)main {
    NSLog(@"Start Working Thread");
    
    ant_tests::ANTSrtTest *test = new ant_tests::ANTSrtTest(antLogFunction);

    test->o_debug = 3;
    test->o_bufsize = 100*1024;
    test->o_send_timeout_ms = 200;
    test->o_timeout = 2000;
    test->o_echo = false;
    if (self.isClient) {
        NSLog(@"Initialize for client");
        test->o_listen = false;
        if (self.serverAddress) {
            test->o_remote_address.push_back(std::string(self.serverAddress.UTF8String));
        }
    } else {
        NSLog(@"Initialize for server");
        test->o_listen = true;
    }

    test->start();
    
    delete test;
    
    NSLog(@"End Working Thread");
}

@end
