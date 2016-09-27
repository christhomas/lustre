//
//  LFSMachine.h
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

@interface LFSMachine : NSObject

- (instancetype)initWithHostname:(NSString *)hostname;
- (BOOL)configure:(NSError **)error;
- (void)invalidate;
- (BOOL)installFilesystem:(NSError **)error;
- (BOOL)installFilesystemKext:(NSError **)error;
- (BOOL)installNetfs:(NSError **)error;
- (BOOL)installFilesystemTest:(NSError **)error;
- (BOOL)uninstallAll:(NSError **)error;
- (NSString *)runTest:(NSString *)name;
- (void)populateError:(NSError **)error message:(NSString *)message;

@end
