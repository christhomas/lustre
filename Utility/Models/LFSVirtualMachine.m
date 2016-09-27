//
//  LFSVirtualMachine.m
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

#import "LFSVirtualMachine.h"
#import "LFSVix.h"

@implementation LFSVirtualMachine {
    LFSVix *        _vix;
    NSString *      _vmxPath;
    NSString *      _snapshot;
}

- (instancetype)initWithVMXPath:(NSString *)vmxPath snapshot:(NSString *)snapshot hostname:(NSString *)hostname
{
    if (self = [super initWithHostname:hostname]) {
        _vmxPath    = [vmxPath copy];
        _snapshot   = [snapshot copy];
    }
    
    return self;
}

- (BOOL)configure:(NSError **)error
{
    if (!_vix) {
        if (![self configureVM]) {
            [self populateError:error message:@"couldn't configure VM"];
            return NO;
        }
    }

    return [super configure:error];
}

- (void)invalidate
{
    _vix = NULL;
    
    [super invalidate];
}

#pragma mark - Private

- (BOOL)configureVM
{
    _vix = [[LFSVix alloc] init];
    if ([_vix connect]) {
        if ([_vix open:_vmxPath]) {
            if ([_vix revert:_snapshot]) {
                return YES;
            } else {
                os_log_error(lfsLogger, "Failed to revert to snapshot");
            }
        } else {
            os_log_error(lfsLogger, "Failed to open VM");
        }
    } else {
        os_log_error(lfsLogger, "Failed to connect to VM");
    }
    
    _vix = nil;

    return NO;
}

@end
