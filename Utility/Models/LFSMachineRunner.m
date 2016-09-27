//
//  LFSMachineRunner.m
//  Utility
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

#import <IOKit/kext/KextManager.h>
#import <sys/sysctl.h>
#import "LFSMachineRunner.h"

@implementation LFSMachineRunner

- (BOOL)createDirectory:(NSString *)path owner:(NSString *)owner group:(NSString *)group permissions:(NSUInteger)permissions;
{
    BOOL        result;
    NSError *   error;
    
    error   = nil;
    result  = [[NSFileManager defaultManager] createDirectoryAtURL:[NSURL fileURLWithPath:path] withIntermediateDirectories:YES attributes:@{ NSFileOwnerAccountName: owner, NSFileGroupOwnerAccountName: group, NSFilePosixPermissions: @(permissions) } error:&error];
    
    if (!result) {
        os_log_error(lfsLogger, "%{public}@", [error localizedDescription]);
    }
    
    return result;
}

- (BOOL)createFile:(NSString *)path owner:(NSString *)owner group:(NSString *)group permissions:(NSUInteger)permissions data:(NSData *)data
{
    BOOL result;
    
    result = [[NSFileManager defaultManager] createFileAtPath:path contents:data attributes:@{ NSFileOwnerAccountName: owner, NSFileGroupOwnerAccountName: group, NSFilePosixPermissions: @(permissions) }];
    
    return result;
}

- (BOOL)copyFile:(NSString *)sourcePath destination:(NSString *)destinationPath owner:(NSString *)owner group:(NSString *)group
{
    NSError * error;
    
    error = nil;
    
    if (![[NSFileManager defaultManager] copyItemAtPath:sourcePath toPath:destinationPath error:&error]) {
        os_log_error(lfsLogger, "%{public}@", [error localizedDescription]);
        return NO;
    }
    
    if (![[NSFileManager defaultManager] setAttributes:@{ NSFileOwnerAccountName: owner, NSFileGroupOwnerAccountName: group } ofItemAtPath:destinationPath error:&error]) {
        os_log_error(lfsLogger, "%{public}@", [error localizedDescription]);
        return NO;
    }
    
    return YES;
}

- (BOOL)execute:(NSString *)path arguments:(NSArray *)arguments
{
    NSTask * task;
    
    task = [NSTask launchedTaskWithLaunchPath:path arguments:arguments];
    [task waitUntilExit];
    if (task.terminationStatus != 0) {
        return NO;
    } else {
        return YES;
    }
}

- (BOOL)removePathItem:(NSString *)path
{
    BOOL        result;
    NSError *   error;
    
    result = [[NSFileManager defaultManager] removeItemAtPath:path error:&error];
    
    if (!result) {
        os_log_error(lfsLogger, "%{public}@", [error localizedDescription]);
    }
    
    return result;
}

- (BOOL)loadExtension:(NSString *)path dependencyPath:(NSString *)dependencyPath
{
    OSReturn result;
    NSArray *   dependencies;
    
    if (dependencyPath) {
        dependencies = @[[NSURL fileURLWithPath:dependencyPath]];
    }
    
    result = KextManagerLoadKextWithURL((__bridge CFURLRef)([NSURL fileURLWithPath:path]), (__bridge CFArrayRef)(dependencies));
    
    return (result == kOSReturnSuccess);
}

- (BOOL)unloadExtension:(NSString *)identifier
{
    OSReturn result;
    
    result = KextManagerUnloadKextWithIdentifier((__bridge CFStringRef)(identifier));
    
    return ((result == kOSReturnSuccess) || (result == 0xdc008006)); // kOSKextReturnNotFound
}

- (NSTimeInterval)heartbeat
{
    NSTimeInterval t;
    
    t = [NSDate timeIntervalSinceReferenceDate];
    
    return t;
}

- (NSString *)runTest:(NSString *)testName
{
    const char *    testNameCString;
    size_t          testNameCStringLength;
    char            resultMessage[220];
    size_t          resultMessageLength;
    int             result;
    
    testNameCString         = [testName UTF8String];
    testNameCStringLength   = strlen(testNameCString);
    resultMessageLength     = 220;
    
    bzero(resultMessage, resultMessageLength);
    
    result = sysctlbyname("lustre.test", NULL, 0, (void *)testNameCString, testNameCStringLength+1);
    if (result != 0) {
        return [NSString stringWithFormat:@"sysctl lustre.test set call failed: %d", errno];
    }
    
    result = sysctlbyname("lustre.test", resultMessage, &resultMessageLength, NULL, 0);
    if (result != 0) {
        return [NSString stringWithFormat:@"sysctl lustre.test get call failed: %d", errno];
    } else {
        return [NSString stringWithUTF8String:resultMessage];
    }
}

@end
