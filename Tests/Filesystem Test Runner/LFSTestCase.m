//
//  LFSTestCase.m
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

#import "LFSTestCase.h"

static LFSVirtualMachine * vm;

@implementation LFSTestCase

+ (void)configureTestRunner:(NSString *)vmxPath snapshot:(NSString *)snapshot hostname:(NSString *)hostname
{
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        vm = [[LFSVirtualMachine alloc] initWithVMXPath:vmxPath snapshot:snapshot hostname:hostname];
    });
}

- (void)setUp
{
    NSError * error;
    
    if (![vm configure:&error]) {
        XCTFail("%@", [error localizedDescription]);
    }
}

- (void)runRemoteTest:(NSString *)name runnerFile:(NSString *)runnerFile runnerLine:(NSUInteger)runnerLine
{
    NSString *  message;
    NSInteger   result;
    NSArray *   parts;
    NSString *  failureMessage;
    NSString *  condition;
    NSString *  file;
    NSUInteger  line;
    
    @try {
        message = [vm runTest:name];
    } @catch (NSException * e) {
        [self handlePossiblePanicInRunnerFile:runnerFile runnerLine:runnerLine];
    }
    
    parts           = [message componentsSeparatedByString:@":"];
    file            = parts[1];
    line            = [parts[2] integerValue];
    result          = [parts[3] integerValue];
    condition       = parts[4];
    failureMessage  = [[parts subarrayWithRange:NSMakeRange(5, parts.count-5)] componentsJoinedByString:@":"];

    if (result != 0) {
        _XCTFailureHandler(self, YES, [file UTF8String], line, condition, @"%@", failureMessage);
    }
}

#pragma mark - Helpers

- (void)handlePossiblePanicInRunnerFile:(NSString *)runnerFile runnerLine:(NSUInteger)runnerLine
{
    NSTask *        task;
    NSArray *       lldbOutput;
    NSUInteger      lineNumber;
    NSUInteger      backtraceStart;
    NSUInteger      backtraceStop;
    NSPipe *        standardOutputPipe;
    NSFileHandle *  standardOutputFileHandle;
    
    lineNumber                  = 0;
    backtraceStart              = 0;
    backtraceStop               = 0;
    standardOutputPipe          = [NSPipe pipe];
    standardOutputFileHandle    = nil;
    task                        = [[NSTask alloc] init];

    [task setLaunchPath:@"/usr/bin/lldb"];
    [task setArguments:@[ @"-o", @"kdp-remote vm0", @"-o", @"bt", @"-o", @"exit" ]];
    [task setStandardOutput:standardOutputPipe];
    
    standardOutputFileHandle = [standardOutputPipe fileHandleForReading];

    [task launch];
    [task waitUntilExit];

    lldbOutput = [[[NSString alloc] initWithData:[standardOutputFileHandle readDataToEndOfFile] encoding:NSUTF8StringEncoding] componentsSeparatedByString:@"\n"];
    
    for (NSString * line in lldbOutput) {
        if ([line hasPrefix:@"(lldb) bt"]) {
            backtraceStart = lineNumber + 1;
        } else if ([line hasPrefix:@"(lldb) exit"]) {
            backtraceStop = lineNumber;
            break;
        }
        lineNumber += 1;
    }
    
    _XCTFailureHandler(self, YES, [runnerFile UTF8String], runnerLine, @"panic", @"%@", [[lldbOutput subarrayWithRange:NSMakeRange(backtraceStart, backtraceStop-backtraceStart)] componentsJoinedByString:@"\n"]);
    
    [vm invalidate];
}

@end
