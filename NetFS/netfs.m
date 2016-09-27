//
//  netfs.m
//  NetFS
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

#import <Foundation/Foundation.h>
#import <NetFS/NetFSPlugin.h>
#import <sys/mount.h>
#import <os/log.h>

#import "LFSMount.h"

os_log_t lfsNetfsLog;

netfsError LFSNetFSCreateSessionRef(void ** sessionRef)
{
    *sessionRef = (void *)CFBridgingRetain([NSObject new]);
    
    return 0;
}

netfsError LFSNetFSGetServerInfo(CFURLRef URL, void * sessionRef, CFDictionaryRef getInfoOptions, CFDictionaryRef * serverParams)
{
    NSMutableDictionary * info;
    
    if (!URL || !sessionRef || !serverParams) {
        os_log_error(lfsNetfsLog, "invalid arguments");
        return EINVAL;
    }

    info = [NSMutableDictionary dictionary];
    
    info[(NSString *)kNetFSServerDisplayNameKey]                = [(__bridge NSURL *)URL host];
    info[(NSString *)kNetFSSupportsChangePasswordKey]           = @(NO);
    info[(NSString *)kNetFSSupportsGuestKey]                    = @(YES);
    info[(NSString *)kNetFSSupportsKerberosKey]                 = @(NO);
    info[(NSString *)kNetFSGuestOnlyKey]                        = @(YES);
    info[(NSString *)kNetFSNoMountAuthenticationKey]            = @(YES);
    info[(NSString *)kNetFSNoUserPreferencesKey]                = @(NO);
    info[(NSString *)kNetFSAllowLoopbackKey]                    = @(NO);

    *serverParams = CFBridgingRetain(info);

    return 0;
}

netfsError LFSNetFSParseURL(CFURLRef URL, CFDictionaryRef * params)
{
    NSMutableDictionary * result;
    
    result = [NSMutableDictionary dictionaryWithCapacity:6];

    if (!URL || !params) {
        os_log_error(lfsNetfsLog, "invalid arguments");
        return EINVAL;
    }
    
    if (((__bridge NSURL *)URL).scheme) {
        result[(NSString *)kNetFSSchemeKey]         = ((__bridge NSURL *)URL).scheme;
    }
    if (((__bridge NSURL *)URL).host) {
        result[(NSString *)kNetFSHostKey]           = ((__bridge NSURL *)URL).host;
    }
    if (((__bridge NSURL *)URL).port) {
        result[(NSString *)kNetFSAlternatePortKey]  = ((__bridge NSURL *)URL).port;
    }
    if (((__bridge NSURL *)URL).path) {
        result[(NSString *)kNetFSPathKey]           = ((__bridge NSURL *)URL).path;
    }
    if (((__bridge NSURL *)URL).user) {
        result[(NSString *)kNetFSUserNameKey]       = ((__bridge NSURL *)URL).user;
    }
    if (((__bridge NSURL *)URL).password) {
        result[(NSString *)kNetFSPasswordKey]       = ((__bridge NSURL *)URL).password;
    }
    
    *params = CFBridgingRetain(result);
    
    return 0;
}

netfsError LFSNetFSCreateURL(CFDictionaryRef URLParams, CFURLRef * URL)
{
    NSURLComponents *   components;
    NSString *          value;
    
    if (!URLParams || !URL) {
        os_log_error(lfsNetfsLog, "invalid arguments");
        return EINVAL;
    }

    components = [[NSURLComponents alloc] init];
    
    value = ((__bridge NSDictionary *)URLParams)[(NSString *)kNetFSSchemeKey];
    if (value) {
        components.scheme = value;
    }
    value = ((__bridge NSDictionary *)URLParams)[(NSString *)kNetFSHostKey];
    if (value) {
        components.host = value;
    }
    value = ((__bridge NSDictionary *)URLParams)[(NSString *)kNetFSAlternatePortKey];
    if (value) {
        components.port = @([value integerValue]);
    }
    value = ((__bridge NSDictionary *)URLParams)[(NSString *)kNetFSPathKey];
    if (value) {
        components.path = value;
    }
    value = ((__bridge NSDictionary *)URLParams)[(NSString *)kNetFSUserNameKey];
    if (value) {
        components.user = value;
    }
    value = ((__bridge NSDictionary *)URLParams)[(NSString *)kNetFSPasswordKey];
    if (value) {
        components.password = value;
    }
    
    *URL = CFBridgingRetain([components URL]);
    
    return  0;
}

netfsError LFSNetFSOpenSession(CFURLRef URL, void * sessionRef, CFDictionaryRef openOptions, CFDictionaryRef * sessionInfo)
{
    NSDictionary * info;
    
    if (!URL || !sessionRef || !sessionInfo) {
        os_log_error(lfsNetfsLog, "invalid arguments");
        return EINVAL;
    }
    
    if ([(__bridge NSURL *)URL user]) {
        info = @{ (NSString *)kNetFSMountedByUserKey:  [(__bridge NSURL *)URL user] };
    } else {
        info = @{ (NSString *)kNetFSMountedByGuestKey: @(YES) };
    }

    *sessionInfo = CFBridgingRetain(info);

    return 0;
}

netfsError LFSNetFSEnumerateShares(void * sessionRef, CFDictionaryRef enumerateOptions, CFDictionaryRef * sharepoints)
{
    return ENOTSUP;
}

netfsError LFSNetFSMount(void * sessionRef, CFURLRef URL, CFStringRef mountPoint, CFDictionaryRef mountOptions, CFDictionaryRef * mountInfo)
{
    netfsError      result;
    NSError *       error;
    LFSMount *      mount;
    NSInteger       mountFlags;
    NSString *      server;
    NSString *      path;

    if (!sessionRef || !URL || !mountPoint) {
        os_log_error(lfsNetfsLog, "invalid arguments");
        return EINVAL;
    }
    
    mountFlags  = [[(__bridge NSDictionary *)mountOptions valueForKey:(NSString *)kNetFSMountFlagsKey] unsignedIntValue];
    server      = [(__bridge NSURL *)URL host];
    path        = [(__bridge NSURL *)URL path];
    error       = nil;

    mount = [[LFSMount alloc] initWithArguments:@[@"NetFS", [NSString stringWithFormat:@"%@:%@", server, path], (__bridge NSString *)mountPoint]];
    
    result = [mount process:&error];
    if (result) {
        result = [mount mount:&error];
        if (result) {
            *mountInfo = CFBridgingRetain(@{ (NSString *)kNetFSMountPathKey: (__bridge NSString *)mountPoint });
        }
    }

    if (error) {
        os_log_error(lfsNetfsLog, "%{public}@", [error localizedDescription]);
        return (netfsError)error.code;
    } else {
        return 0;
    }
}

netfsError LFSNetFSCancel(void *in_SessionRef)
{
    return 0;
}

netfsError LFSNetFSCloseSession(void * sessionRef)
{
    NSObject * object;
    
    object = CFBridgingRelease(sessionRef);
    object = nil;
    
    return 0;
}

netfsError LFSNetFSGetMountInfo(CFStringRef mountpoint, CFDictionaryRef * mountInfo)
{
    NSDictionary *  result;
    struct statfs   statbuf;
    
    result = nil;
    
    if (!mountpoint || !mountInfo) {
        os_log_error(lfsNetfsLog, "invalid arguments");
        return EINVAL;
    }

    if (statfs([(__bridge NSString *)mountpoint cStringUsingEncoding:NSUTF8StringEncoding], &statbuf) == -1) {
        os_log_error(lfsNetfsLog, "statfs failed: %d", errno);
        return errno;
    } else {
        *mountInfo = CFBridgingRetain(@{ (NSString *)kNetFSMountedURLKey:  [NSString stringWithCString:statbuf.f_mntfromname encoding:NSASCIIStringEncoding] });
        return 0;
    }
}

static NetFSMountInterface_V1 lfs_netfs_mount_interface_table = {
    NULL,                       // IUNKNOWN_C_GUTS: _reserved
    NetFSQueryInterface,		// IUNKNOWN_C_GUTS: QueryInterface
    NetFSInterface_AddRef,		// IUNKNOWN_C_GUTS: AddRef
    NetFSInterface_Release,		// IUNKNOWN_C_GUTS: Release
    LFSNetFSCreateSessionRef,
    LFSNetFSGetServerInfo,
    LFSNetFSParseURL,
    LFSNetFSCreateURL,
    LFSNetFSOpenSession,
    LFSNetFSEnumerateShares,
    LFSNetFSMount,
    LFSNetFSCancel,
    LFSNetFSCloseSession,
    LFSNetFSGetMountInfo,
};

void * LFSNetFSInterfaceFactory(CFAllocatorRef allocator, CFUUIDRef typeID)
{
    CFUUIDRef uuid;
    
    if (CFEqual(typeID, kNetFSTypeID)) {
        uuid        = CFUUIDGetConstantUUIDWithBytes(allocator, 0x9A, 0xB3, 0x85, 0xF1, 0xE4, 0x4A, 0x4F, 0x9B, 0x9D, 0x83, 0xC8, 0xF8, 0xB8, 0x8C, 0x77, 0xB8);
        lfsNetfsLog = os_log_create("com.ciderapps.lustre.NetFS", "plugin");
        
        return NetFS_CreateInterface(uuid, &lfs_netfs_mount_interface_table);
    } else {
        return NULL;
    }
}
