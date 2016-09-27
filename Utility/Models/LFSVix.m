//
//  LFSVix.h
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

#import "LFSVix.h"
#import "vix.h"

@implementation LFSVix {
    VixHandle _hostHandle;
    VixHandle _vmHandle;
}

- (BOOL)connect
{
    VixError    err;
    VixHandle   jobHandle;
    
    jobHandle   = VixHost_Connect(VIX_API_VERSION, VIX_SERVICEPROVIDER_VMWARE_WORKSTATION, "", 0, "", "", 0, VIX_INVALID_HANDLE, NULL, NULL);
    err         = VixJob_Wait(jobHandle, VIX_PROPERTY_JOB_RESULT_HANDLE, &_hostHandle, VIX_PROPERTY_NONE);
    if (VIX_FAILED(err)) {
        os_log_error(lfsLogger, "%{public}s", Vix_GetErrorText(err, NULL));
        return NO;
    }
    
    return YES;
}

- (BOOL)open:(NSString *)vmxFile
{
    VixError    err;
    VixHandle   jobHandle;

    jobHandle   = VixVM_Open(_hostHandle, [vmxFile cStringUsingEncoding:NSASCIIStringEncoding], NULL, NULL);
    err         = VixJob_Wait(jobHandle, VIX_PROPERTY_JOB_RESULT_HANDLE, &_vmHandle, VIX_PROPERTY_NONE);
    
    if (VIX_FAILED(err)) {
        os_log_error(lfsLogger, "%{public}s", Vix_GetErrorText(err, NULL));
        return NO;
    }
    
    return YES;
}

- (BOOL)revert:(NSString *)name
{
    VixHandle   snapshotHandle;
    VixError    err;
    VixHandle   jobHandle;

    snapshotHandle = VIX_INVALID_HANDLE;
    
    err = VixVM_GetNamedSnapshot(_vmHandle, [name UTF8String], &snapshotHandle);
    if (VIX_FAILED(err)) {
        os_log_error(lfsLogger, "%{public}s", Vix_GetErrorText(err, NULL));
        return NO;
    }
    
    jobHandle   = VixVM_RevertToSnapshot(_vmHandle, snapshotHandle, VIX_VMPOWEROP_LAUNCH_GUI, VIX_INVALID_HANDLE, NULL, NULL);
    err         = VixJob_Wait(jobHandle, VIX_PROPERTY_NONE);

    while (err == VIX_E_UNFINISHED_JOB) {
        err = VixJob_Wait(jobHandle, VIX_PROPERTY_NONE);
    }
    
    Vix_ReleaseHandle(snapshotHandle);
    if (VIX_FAILED(err)) {
        os_log_error(lfsLogger, "%{public}s", Vix_GetErrorText(err, NULL));
        return NO;
    }
    
    jobHandle   = VixVM_WaitForToolsInGuest(_vmHandle, 120, NULL, NULL);
    err         = VixJob_GetError(jobHandle);
    
    while (err == VIX_E_UNFINISHED_JOB) {
        err = VixJob_Wait(jobHandle, VIX_PROPERTY_NONE);
    }
    
    if (VIX_FAILED(err)) {
        os_log_error(lfsLogger, "%{public}s", Vix_GetErrorText(err, NULL));
        return NO;
    }
    
    return YES;
}

- (void)close
{
    Vix_ReleaseHandle(_vmHandle);
    VixHost_Disconnect(_hostHandle);
    Vix_ReleaseHandle(_hostHandle);
}

- (BOOL)copyFile:(NSString *)source destination:(NSString *)destination
{
    VixError    err;
    VixHandle   jobHandle;
    
    jobHandle   = VixVM_CopyFileFromHostToGuest(_vmHandle, [source fileSystemRepresentation], [destination fileSystemRepresentation], 0, VIX_INVALID_HANDLE, NULL, NULL);
    err         = VixJob_Wait(jobHandle, VIX_PROPERTY_NONE);
    
    if (VIX_FAILED(err)) {
        os_log_error(lfsLogger, "%{public}s", Vix_GetErrorText(err, NULL));
        return NO;
    }
    
    return YES;
}

- (BOOL)deleteFile:(NSString *)file
{
    VixError    err;
    VixHandle   jobHandle;
    
    jobHandle   = VixVM_DeleteFileInGuest(_vmHandle, [file fileSystemRepresentation], NULL, NULL);
    err         = VixJob_Wait(jobHandle, VIX_PROPERTY_NONE);
    
    if (VIX_FAILED(err)) {
        os_log_error(lfsLogger, "%{public}s", Vix_GetErrorText(err, NULL));
        return NO;
    }
    
    return YES;
}

- (BOOL)login:(NSString *)username password:(NSString *)password
{
    VixError    err;
    VixHandle   jobHandle;

    jobHandle   = VixVM_LoginInGuest(_vmHandle, [username UTF8String], [password UTF8String], VIX_LOGIN_IN_GUEST_REQUIRE_INTERACTIVE_ENVIRONMENT, NULL, NULL);
    err         = VixJob_Wait(jobHandle, VIX_PROPERTY_NONE);
    
    if (VIX_FAILED(err)) {
        os_log_error(lfsLogger, "%{public}s", Vix_GetErrorText(err, NULL));
        return NO;
    }
    
    return YES;
}

- (BOOL)logout
{
    VixError    err;
    VixHandle   jobHandle;
    
    jobHandle   = VixVM_LogoutFromGuest(_vmHandle, NULL, NULL);
    err         = VixJob_Wait(jobHandle, VIX_PROPERTY_NONE);
    
    if (VIX_FAILED(err)) {
        os_log_error(lfsLogger, "%{public}s", Vix_GetErrorText(err, NULL));
        return NO;
    }
    
    return YES;
}

- (BOOL)runScript:(NSString *)path arguments:(NSString *)arguments
{
    return [self run:path arguments:arguments options:0];
}

- (BOOL)runProgram:(NSString *)path arguments:(NSString *)arguments
{
    return [self run:path arguments:arguments options:VIX_RUNPROGRAM_ACTIVATE_WINDOW];
}

- (BOOL)run:(NSString *)path arguments:(NSString *)arguments options:(VixRunProgramOptions)options
{
    VixError    err;
    VixHandle   jobHandle;

    jobHandle   = VixVM_RunProgramInGuest(_vmHandle, [path fileSystemRepresentation], [arguments UTF8String], options, VIX_INVALID_HANDLE, NULL, NULL);
    err         = VixJob_Wait(jobHandle, VIX_PROPERTY_NONE);
    
    if (VIX_FAILED(err)) {
        os_log_error(lfsLogger, "%{public}s", Vix_GetErrorText(err, NULL));
        return NO;
    }
    
    return YES;
}

@end
