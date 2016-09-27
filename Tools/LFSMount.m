//
//  LFSMount.m
//  Mount
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

#import <sys/mount.h>

#import "LFSMount.h"
#import "constants.h"
#import "mount_args.h"

@implementation LFSMount {
    NSArray *           _arguments;
    
    NSArray *           _options;
    NSMutableArray *    _managementServers;
    NSString *          _filesystem;
    NSString *          _mountPoint;
}

#pragma mark - Lifecycle

- (instancetype)initWithArguments:(NSArray *)arguments
{
    if (self = [super init]) {
        _arguments          = [arguments copy];
        _managementServers  = [NSMutableArray arrayWithCapacity:4];
    }
    
    return self;
}

#pragma mark - Public

- (BOOL)process:(NSError **)error
{
    BOOL result;
    
    do {
        result = [self parseArguments:error];
        if (!result) {
            break;
        }
        
        result = [self processOptions:error];
        if (!result) {
            break;
        }
        
        result = [self processManagementServers:error];
        if (!result) {
            break;
        }
        
        result = [self processFilesystem:error];
        if (!result) {
            break;
        }

        result = [self processMountPoint:error];
        if (!result) {
            break;
        }
    } while (FALSE);
    
    return result;
}

- (BOOL)mount:(NSError **)error
{
    BOOL                     	result;
    int32_t                     err;
    int32_t                     flags;
    struct lustre_mount_args    mountArgs;
    const char *                realMountPoint;
    const char *                address;
    const char *                label;
    
    result              = YES;
    flags               = 0;
    realMountPoint      = [_mountPoint cStringUsingEncoding:NSUTF8StringEncoding];
    address             = [[(NSHost *)[_managementServers firstObject] address] cStringUsingEncoding:NSUTF8StringEncoding];
    label               = [_filesystem cStringUsingEncoding:NSUTF8StringEncoding];
    
    strlcpy(mountArgs.host,     address,    kLustreMountArgsHostMax);
    strlcpy(mountArgs.label,    label,      kLustreMountArgsLabelMax);

    err = mount(kLustreFilesystemName, realMountPoint, flags, &mountArgs);
    if (err < 0) {
        // If we get the error that indicates that our KEXT isn't loaded; load the KEXT and try again.
        if (errno == ENODEV) {
            if ([self loadKext:error]) {
                err = mount(kLustreFilesystemName, realMountPoint, flags, &mountArgs);
                if (err < 0) {
                    result = NO;
                    [self populateError:error code:errno message:@"Failed to mount 1"];
                }
            }
        } else {
            result = NO;
            [self populateError:error code:errno message:@"Failed to mount 2"];
        }
    }
    
    return result;
}

#pragma mark - Private

- (BOOL)parseArguments:(NSError **)error
{
    NSUInteger  index;
    NSArray *   remoteMountPointParts;
    NSArray *   managementServerStrings;
    NSHost *    server;
    
    if ((_arguments.count < 3) || (_arguments.count > 5)) {
        [self populateError:error code:EXIT_FAILURE message:@"Incorrect number of arguments"];
        return NO;
    }

    index = 1;
    
    if ([[_arguments objectAtIndex:index] isEqualToString:@"-o"]) {
        _options = [[_arguments objectAtIndex:1] componentsSeparatedByString:@","];
        index = 3;
    } else {
        index = 1;
    }
    
    remoteMountPointParts = [[_arguments objectAtIndex:index] componentsSeparatedByString:@":"];
    if (remoteMountPointParts.count < 2) {
        [self populateError:error code:EXIT_FAILURE message:[NSString stringWithFormat:@"Bad device: %@", [_arguments objectAtIndex:index]]];
        return NO;
    }
    
    _filesystem = [remoteMountPointParts lastObject];
    
    managementServerStrings = [remoteMountPointParts subarrayWithRange:NSMakeRange(0, remoteMountPointParts.count-1)];
    for (NSString * serverString in managementServerStrings) {
        server = [NSHost hostWithName:serverString];
        if (!server || !server.address) {
            server = [NSHost hostWithAddress:serverString];
        }
        if (server && server.address) {
            [_managementServers addObject:server];
        } else {
            [self populateError:error code:EXIT_FAILURE message:@"management server not found"];
            return NO;
        }
    }
    
    _mountPoint = [_arguments lastObject];
    
    return YES;
}

- (BOOL)processOptions:(NSError **)error
{
    if ([_options count] != 0) {
        [self populateError:error code:EXIT_FAILURE message:@"options are not supported"];
        return NO;
    }
    
    return YES;
}

- (BOOL)processManagementServers:(NSError **)error
{
    if (_managementServers.count == 0) {
        [self populateError:error code:EXIT_FAILURE message:@"no management server(s) not found"];
        return NO;
    }

    return YES;
}

- (BOOL)processFilesystem:(NSError **)error
{
    if (![_filesystem hasPrefix:@"/"]) {
        [self populateError:error code:EXIT_FAILURE message:@"filesystem isn't valid"];
        return NO;
    }
    _filesystem = [_filesystem substringFromIndex:1];

    return YES;
}

- (BOOL)processMountPoint:(NSError **)error
{
    BOOL isDirectory;
    
    if ([[NSFileManager defaultManager] fileExistsAtPath:_mountPoint isDirectory:&isDirectory]) {
        if (isDirectory) {
            _mountPoint = [[_mountPoint stringByStandardizingPath] stringByResolvingSymlinksInPath];
            return YES;
        } else {
            [self populateError:error code:EXIT_FAILURE message:@"mount point isn't a directory"];
            return NO;
        }
    } else {
        [self populateError:error code:EXIT_FAILURE message:@"mount point doesn't exist"];
        return NO;
    }
}

- (BOOL)loadKext:(NSError **)error
{
    NSTask * task;
    
    task = [NSTask launchedTaskWithLaunchPath:@"/sbin/kextload" arguments:@[@"/Library/Extensions/Lustre.kext"]];
    
    [task waitUntilExit];
    
    if (task.terminationStatus == 0) {
        return YES;
    } else {
        return NO;
    }
}

- (void)populateError:(NSError **)error code:(NSInteger)code message:(NSString *)message
{
    if (error) {
        *error = [NSError errorWithDomain:@"LFSMount" code:code userInfo:@{ NSLocalizedDescriptionKey: message }];
    }
}

@end
