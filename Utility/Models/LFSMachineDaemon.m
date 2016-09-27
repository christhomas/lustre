//
//  LFSMachineDaemon.m
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

#import "LFSMachineDaemon.h"
#import "LFSMachineRunner.h"

@implementation LFSVirtualMachineDaemon {
    NSSocketPort *      _port;
    LFSMachineRunner *  _testRunner;
    NSConnection *      _connection;
}

- (BOOL)start
{
    BOOL result;
    
    _testRunner = [[LFSMachineRunner alloc] init];
    _port       = [[NSSocketPort alloc] init];
    _connection = [NSConnection connectionWithReceivePort:_port sendPort:nil];
    [_connection setRootObject:_testRunner];
    result      = [[NSSocketPortNameServer sharedInstance] registerPort:_port name:NSStringFromProtocol(@protocol(LFSMachineProtocol))];
    
    return result;
}

- (void)stop
{
    [_connection invalidate];
    [[NSSocketPortNameServer sharedInstance] removePortForName:NSStringFromProtocol(@protocol(LFSMachineProtocol))];
    [_port invalidate];
}

@end
