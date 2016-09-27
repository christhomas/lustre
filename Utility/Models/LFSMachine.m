//
//  LFSMachine.m
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

#import "LFSMachine.h"
#import "LFSMachineRunner.h"

NSString * kLFSMachineFilesystem                    = @"lustre.fs";
NSString * kLFSMachineFilesystemKext                = @"Lustre.kext";
NSString * kLFSMachineNetfs                         = @"lustre.bundle";
NSString * kLFSMachineFilesystemTestKext            = @"lustre-test.kext";
NSString * kLFSMachineFilesystemsPath               = @"/Library/Filesystems";
NSString * kLFSMachineExtensionsPath                = @"/Library/Extensions";
NSString * kLFSMachineNetFSPath                     = @"/Library/Filesystems/NetFSPlugins";
NSString * kLFSMachineFilesystemKextIdentifier      = @"com.ciderapps.lustre.Extension";
NSString * kLFSMachineFilesystemTestKextIdentifier  = @"com.ciderapps.lustre.Test";

@implementation LFSMachine {
    NSString *      _hostname;
    id              _testRunnerProxy;
    NSSocketPort *  _port;
    NSConnection *  _connection;
}

- (instancetype)initWithHostname:(NSString *)hostname
{
    if (self = [super init]) {
        _hostname   = [hostname copy];
    }
    
    return self;
}

- (BOOL)configure:(NSError **)error
{
    NSTimeInterval  t;
    NSUInteger      retryCount;
    
    t           = 0;
    retryCount  = 120;
    
    if (!_testRunnerProxy) {
        do {
            [NSThread sleepForTimeInterval:2.0];
            retryCount  -= 1;
            
            @try {
                [self configureTestRunner];
                t = [_testRunnerProxy heartbeat];
                if ((t != 0) && [self uninstallAll:error]) {
                    if ([self installFilesystem:error]) {
                        if ([self installFilesystemKext:error]) {
                            if ([self installNetfs:error]) {
                                [NSThread sleepForTimeInterval:3.0];
                                if (![self installFilesystemTest:error]) {
                                    t = 0;
                                }
                            }
                        }
                    }
                }
            } @catch (NSException * e) {
                t = 0;
            }
        } while ((t == 0) && (retryCount > 0));
    } else {
        t = [_testRunnerProxy heartbeat];
    }
    
    if (t == 0) {
        [self populateError:error message:@"couldn't get machine heartbeat"];
        return NO;
    } else {
        return YES;
    }
}

- (void)invalidate
{
    _testRunnerProxy = NULL;
}

- (NSString *)runTest:(NSString *)name
{
    return [_testRunnerProxy runTest:name];
}

- (BOOL)installFilesystem:(NSError **)error
{
    BOOL result;
    
    result = [self copyBuildArtifact:kLFSMachineFilesystem destination:[kLFSMachineFilesystemsPath stringByAppendingPathComponent:kLFSMachineFilesystem] error:error];

    return result;
}

- (BOOL)installFilesystemKext:(NSError **)error
{
    BOOL result;
    
    result = [self copyBuildArtifact:kLFSMachineFilesystemKext destination:[kLFSMachineExtensionsPath stringByAppendingPathComponent:kLFSMachineFilesystemKext] error:error];
    
    if (result) {
        result = [_testRunnerProxy loadExtension:[kLFSMachineExtensionsPath stringByAppendingPathComponent:kLFSMachineFilesystemKext] dependencyPath:nil];
        if (!result) {
            [self populateError:error message:@"Failed to load filesystem extension"];
        }
    }
    
    return result;
}

- (BOOL)installNetfs:(NSError **)error
{
    BOOL result;
    
    result = [self copyBuildArtifact:kLFSMachineNetfs destination:[kLFSMachineNetFSPath stringByAppendingPathComponent:kLFSMachineNetfs] error:error];
    
    return result;
}

- (BOOL)installFilesystemTest:(NSError **)error
{
    BOOL result;
    
    result = [self copyBuildArtifact:kLFSMachineFilesystemTestKext destination:[kLFSMachineExtensionsPath stringByAppendingPathComponent:kLFSMachineFilesystemTestKext] error:error];
    
    if (result) {
        result = [_testRunnerProxy loadExtension:[kLFSMachineExtensionsPath stringByAppendingPathComponent:kLFSMachineFilesystemTestKext] dependencyPath:kLFSMachineExtensionsPath];
        if (!result) {
            [self populateError:error message:@"Failed to load filesystem test extension"];
        }
    }
    
    return result;
}

- (BOOL)uninstallAll:(NSError **)error
{
    BOOL result;
    
    do {
        result = [_testRunnerProxy unloadExtension:kLFSMachineFilesystemTestKextIdentifier];
        if (!result) {
            break;
        }

        result = [_testRunnerProxy unloadExtension:kLFSMachineFilesystemKextIdentifier];
        if (!result) {
            break;
        }
        
        result = [_testRunnerProxy execute:@"/bin/rm" arguments:@[@"-rf", [kLFSMachineNetFSPath stringByAppendingPathComponent:kLFSMachineNetfs]]];
        if (!result) {
            break;
        }

        result = [_testRunnerProxy execute:@"/bin/rm" arguments:@[@"-rf", [kLFSMachineExtensionsPath stringByAppendingPathComponent:kLFSMachineFilesystemTestKext]]];
        if (!result) {
            break;
        }

        result = [_testRunnerProxy execute:@"/bin/rm" arguments:@[@"-rf", [kLFSMachineExtensionsPath stringByAppendingPathComponent:kLFSMachineFilesystemKext]]];
        if (!result) {
            break;
        }

        result = [_testRunnerProxy execute:@"/bin/rm" arguments:@[@"-rf", [kLFSMachineFilesystemsPath stringByAppendingPathComponent:kLFSMachineFilesystemKext]]];
        if (!result) {
            break;
        }
    } while (FALSE);
    
    return result;
}

#pragma mark - Notifications

- (void)connectionDidDie:(NSNotification *)notification
{
    _testRunnerProxy = nil;
}

#pragma mark - Private

- (void)configureTestRunner
{
    _port           = (NSSocketPort *)[[NSSocketPortNameServer sharedInstance] portForName:NSStringFromProtocol(@protocol(LFSMachineProtocol)) host:_hostname];
    _connection     = [NSConnection connectionWithReceivePort:nil sendPort:_port];
    
    [_connection setRequestTimeout:5.0];
    [_connection setReplyTimeout:5.0];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(connectionDidDie:) name:NSConnectionDidDieNotification object:_connection];
    
    _testRunnerProxy = [_connection rootProxy];
    
    [_testRunnerProxy setProtocolForProxy:@protocol(LFSMachineProtocol)];
}

- (BOOL)copyResource:(NSString *)resource module:(NSString *)module destination:(NSString *)destination error:(NSError **)error
{
    NSDictionary *  environment;
    NSString *      modulePath;
    NSString *      resourcePath;
    NSURL *         resourceURL;
    NSData *        data;
    NSDictionary *  resourceAttributes;
    BOOL            result;
    
    environment     = [[NSProcessInfo processInfo] environment];
    modulePath      = [[[environment[@"PROJECT_DIR"] stringByAppendingPathComponent:@"Modules"] stringByAppendingPathComponent:module] stringByAppendingPathComponent:@"Mac"];
    resourcePath    = [modulePath stringByAppendingPathComponent:resource];
    
    resourceURL = [NSURL fileURLWithPath:resourcePath];
    data        = [NSData dataWithContentsOfURL:resourceURL options:0 error:error];
    if (*error) {
        return NO;
    }
    
    resourceAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:[resourceURL path] error:error];
    if (*error) {
        return NO;
    }
    
    result = [_testRunnerProxy createFile:destination owner:@"root" group:@"wheel" permissions:resourceAttributes.filePosixPermissions data:data];
    if (!result) {
        [self populateError:error message:[NSString stringWithFormat:@"Failed to copy resource: %@", resource]];
    }
    
    return result;
}

- (BOOL)copyBuildArtifact:(NSString *)artifact destination:(NSString *)destination error:(NSError **)error
{
    NSURL *                 artifactURL;
    NSURL *                 buildURL;
    NSDirectoryEnumerator * enumerator;
    NSString *              fileType;
    NSString *              path;
    NSData *                data;
    NSDictionary *          artifactAttributes;
    BOOL                    result;
    
    buildURL    = [self buildURL];
    artifactURL = [buildURL URLByAppendingPathComponent:artifact];
    
    [artifactURL getResourceValue:&fileType forKey:NSURLFileResourceTypeKey error:error];
    if (*error) {
        return NO;
    }
    
    if ([fileType isEqualToString:NSURLFileResourceTypeRegular]) {
        data = [NSData dataWithContentsOfURL:artifactURL options:0 error:error];
        if (*error) {
            return NO;
        }
        artifactAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:[artifactURL path] error:error];
        if (*error) {
            return NO;
        }
        
        [_testRunnerProxy createDirectory:[destination stringByDeletingLastPathComponent] owner:@"root" group:@"wheel" permissions:0755];
        
        
        result = [_testRunnerProxy createFile:destination owner:@"root" group:@"wheel" permissions:artifactAttributes.filePosixPermissions data:data];
        if (!result) {
            [self populateError:error message:[NSString stringWithFormat:@"Failed to copy file: %@", artifact]];
        }
    } else {
        enumerator = [[NSFileManager defaultManager] enumeratorAtURL:artifactURL includingPropertiesForKeys:@[NSURLFileResourceTypeKey, NSFilePosixPermissions] options:0 errorHandler:^BOOL(NSURL * url, NSError * error) {
            return NO;
        }];
        
        for (NSURL * url in enumerator) {
            [url getResourceValue:&fileType forKey:NSURLFileResourceTypeKey error:error];
            if (*error) {
                return NO;
            }
            artifactAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:[url path] error:error];
            if (*error) {
                return NO;
            }
            
            path = [[url path] stringByReplacingOccurrencesOfString:[buildURL path] withString:[destination stringByDeletingLastPathComponent]];
            
            if ([fileType isEqualToString:NSURLFileResourceTypeRegular]) {
                data = [NSData dataWithContentsOfURL:url options:0 error:error];
                if (*error) {
                    return NO;
                }
                result = [_testRunnerProxy createFile:path owner:@"root" group:@"wheel" permissions:artifactAttributes.filePosixPermissions data:data];
                if (!result) {
                    [self populateError:error message:[NSString stringWithFormat:@"Failed to create file: %@", path]];
                    return NO;
                }
            } else {
                result = [_testRunnerProxy createDirectory:path owner:@"root" group:@"wheel" permissions:artifactAttributes.filePosixPermissions];
                if (!result) {
                    [self populateError:error message:[NSString stringWithFormat:@"Failed to create directory: %@", path]];
                    return NO;
                }
            }
        }
    }
    
    return YES;
}

- (NSURL *)buildURL
{
    NSString *      buildPath;
    NSDictionary *  environment;
    
    environment = [[NSProcessInfo processInfo] environment];
    buildPath   = environment[@"__XCODE_BUILT_PRODUCTS_DIR_PATHS"];
    
    return [NSURL fileURLWithPath:buildPath];
}

- (void)populateError:(NSError **)error message:(NSString *)message
{
    if (error) {
        *error = [NSError errorWithDomain:@"LFSMachine" code:0 userInfo:@{ NSLocalizedDescriptionKey: message }];
    }
}

@end
