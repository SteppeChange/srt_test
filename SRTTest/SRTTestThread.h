//
//  SRTTestThread.h
//  SRTTest
//
//  Created by Alexey on 10/05/2019.
//  Copyright © 2019 Steppechange. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface SRTTestThread : NSThread

@property (atomic, assign) BOOL isClient;
@property (atomic, strong, nullable) NSString *serverAddress;

@end

NS_ASSUME_NONNULL_END
