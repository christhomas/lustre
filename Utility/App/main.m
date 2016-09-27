//
//  main.m
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

#import <Cocoa/Cocoa.h>
#import "LFSMachineDaemon.h"
#import "LFSVirtualMachine.h"
#import "LFSRealMachine.h"

os_log_t lfsLogger;

int main(int argc, const char * argv[])
{
    @autoreleasepool {
        LFSVirtualMachineDaemon *   daemon;
        __block NSArray *           arguments;
        
        
        lfsLogger = os_log_create("com.ciderapps.lustre.Utility", "default");
        arguments = [[NSProcessInfo processInfo] arguments];
        
        if ([arguments containsObject:@"-daemon"]) {
            daemon = [[LFSVirtualMachineDaemon alloc] init];
            
            if ([daemon start]) {
                [[NSRunLoop currentRunLoop] run];
                [daemon stop];
                return EXIT_SUCCESS;
            } else {
                return EXIT_FAILURE;
            }
        } else if ([arguments containsObject:@"-run"]) {
            LFSMachine *    machine;
            NSError *       error;

            error = nil;
            
            if ([arguments containsObject:@"vm"]) {
                machine = [[LFSVirtualMachine alloc] initWithVMXPath:@UTILITY_VM snapshot:@UTILITY_SNAPSHOT hostname:@UTILITY_HOSTNAME];
            } else if ([arguments containsObject:@"machine"]) {
                machine = [[LFSRealMachine alloc] initWithHostname:@UTILITY_HOSTNAME];
            }
            
            if (![machine configure:&error]) {
                os_log_error(lfsLogger, "%{public}@", [error localizedDescription]);
                exit(EXIT_FAILURE);
            }

            return NSApplicationMain(argc, argv);
        } else if ([arguments containsObject:@"-test"]) {
            return NSApplicationMain(argc, argv);
        }
    }
}
