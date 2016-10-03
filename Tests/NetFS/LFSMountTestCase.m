//
//  LFSMountTestCase.m
//  Filesystem Test Runner
//
//  Lustre Filesystem For macOS
//  Copyright (C) 2016 Cider Apps, LLC.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#import <XCTest/XCTest.h>
#import "LFSMount.h"

@interface LFSMountTestCase : XCTestCase

@end

@implementation LFSMountTestCase

- (void)testArguments
{
    [self helperTestArguments:@[@"Test", @"10.10.1.10:/fs1", @"/tmp"] expectSuccess:YES];
    [self helperTestArguments:@[@"Test", @"10.10.1.10:10.10.1.11:/fs1", @"/tmp"] expectSuccess:YES];
    [self helperTestArguments:@[@"Test", @"lustre.org:10.10.1.11:apple.com:/fs1", @"/tmp"] expectSuccess:YES];

    [self helperTestArguments:@[@"Test", @"-o", @"flock,user_xattr", @"10.10.1.10:10.10.1.11:10.10.1.12:/fs1", @"/tmp"] expectSuccess:NO];
    [self helperTestArguments:@[@"Test", @"-o", @"flock,user_xattr,quota=10", @"10.10.1.10:/fs1", @"/tmp"] expectSuccess:NO];

    [self helperTestArguments:@[@"Test", @"10.10.1.10:/fs1", @"/bad-mp"] expectSuccess:NO];
    [self helperTestArguments:@[@"Test"] expectSuccess:NO];
    [self helperTestArguments:@[] expectSuccess:NO];
    [self helperTestArguments:@[@"Test", @"10.10.1.10:/tmp"] expectSuccess:NO];
    [self helperTestArguments:@[@"Test", @"/tmp"] expectSuccess:NO];
    [self helperTestArguments:@[@"Test", @"10.10.1.10", @"/tmp"] expectSuccess:NO];
    [self helperTestArguments:@[@"Test", @"/fs1", @"/tmp"] expectSuccess:NO];
    [self helperTestArguments:@[@"Test", @"10.10.1.10/fs1", @"/tmp"] expectSuccess:NO];
    [self helperTestArguments:@[@"Test", @"lustre.no:10.10.1.11:/fs1", @"/tmp"] expectSuccess:NO];
}

#pragma mark - Private

- (void)helperTestArguments:(NSArray *)arguments expectSuccess:(BOOL)expectSuccess
{
    LFSMount *  mount;
    NSError *   error;
    BOOL        result;
    
    error   = nil;
    mount   = [[LFSMount alloc] initWithArguments:arguments];
    result  = [mount process:&error];
    
    if (expectSuccess) {
        XCTAssertNil(error);
        XCTAssertTrue(result);
    } else {
        XCTAssertNotNil(error);
        XCTAssertFalse(result);
    }
}

@end
