//
//  LFSMachineRunner.h
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

#import <Foundation/Foundation.h>

@protocol LFSMachineProtocol <NSObject>

- (BOOL)createDirectory:(NSString *)path owner:(NSString *)owner group:(NSString *)group permissions:(NSUInteger)permissions;
- (BOOL)createFile:(NSString *)path owner:(NSString *)owner group:(NSString *)group permissions:(NSUInteger)permissions data:(NSData *)data;
- (BOOL)copyFile:(NSString *)sourcePath destination:(NSString *)destinationPath owner:(NSString *)owner group:(NSString *)group;
- (BOOL)execute:(NSString *)path arguments:(NSArray *)arguments;
- (BOOL)removePathItem:(NSString *)path;
- (BOOL)loadExtension:(NSString *)path dependencyPath:(NSString *)dependencyPath;
- (BOOL)unloadExtension:(NSString *)identifier;
- (NSTimeInterval)heartbeat;
- (NSString *)runTest:(NSString *)testName;

@end

@interface LFSMachineRunner : NSObject <LFSMachineProtocol>

@end
