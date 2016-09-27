//
//  test.c
//  Filesystem Test
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

#include <sys/sysctl.h>
#include "test.h"
#include "test_listings.h"

const uint8_t kLustreTestSysctlMessageSize = kLustreTestResultMessageSize + 20;

char lustre_test_message[kLustreTestSysctlMessageSize];

int lustre_test_sysctl_handler SYSCTL_HANDLER_ARGS
{
    int                                 error;
    char                                sectionName[kLustreTestSysctlMessageSize];
    char                                testName[kLustreTestSysctlMessageSize];
    const struct lustre_test_listing *  testListing;
    enum lustre_test_result             testResult;
    char                                testMessage[kLustreTestResultMessageSize];
    char                                newTestMessage[kLustreTestSysctlMessageSize];
    boolean_t                           sectionNameFound;
    boolean_t                           testNameFound;
    char                                file[kLustreTestResultFileSize];
    uint32_t                            line;
    char                                testCondition[kLustreTestResultMessageSize];
    
    sectionNameFound    = FALSE;
    testNameFound       = FALSE;
    testResult          = kLustreTestResultSuccess;
    
    bzero(testMessage,      kLustreTestResultMessageSize);
    bzero(testCondition,    kLustreTestResultMessageSize);
    bzero(newTestMessage,   kLustreTestSysctlMessageSize);
    
    error = sysctl_handle_string(oidp, oidp->oid_arg1, oidp->oid_arg2, req);
    if (!error && req->newptr) {
        for (uint32_t i=0; i<kLustreTestSysctlMessageSize; i++) {
            if (!sectionNameFound) {
                if (lustre_test_message[i] == '.') {
                    sectionName[i] = '\0';
                    sectionNameFound = TRUE;
                } else {
                    sectionName[i] = lustre_test_message[i];
                }
            } else {
                if (lustre_test_message[i] == '\0') {
                    testNameFound = TRUE;
                    testName[i-(strlen(sectionName)+1)] = '\0';
                    break;
                } else {
                    testName[i-(strlen(sectionName)+1)] = lustre_test_message[i];
                }
            }
        }
        
        if (!sectionNameFound || !testNameFound) {
            snprintf(newTestMessage, kLustreTestSysctlMessageSize, "%s:%s:%u:%d:%s:%s", lustre_test_message, "-", 0, kLustreTestResultError, "-", "bad test section.name");
            return error;
        }
        
        sectionNameFound  = FALSE;
        testNameFound     = FALSE;

        for (uint32_t i=0; i<kLustreTestListingsCount; i++) {
            testListing = &kLustreTestListings[i];
            if (strprefix(lustre_test_message, testListing->section) == 1) {
                sectionNameFound = TRUE;
                if (strcmp(testName, testListing->name) == 0) {
                    (*testListing->test)(&testResult, testCondition, testMessage, file, &line);
                    snprintf(newTestMessage, kLustreTestSysctlMessageSize, "%s:%s:%u:%d:%s:%s", lustre_test_message, file, line, testResult, testCondition, testMessage);
                    testNameFound = TRUE;
                    break;
                }
            } else {
                testListing = NULL;
            }
        }
        if (!sectionNameFound) {
            snprintf(newTestMessage, kLustreTestSysctlMessageSize, "%s:%s:%u:%d:%s:%s - %s", lustre_test_message, "-", 0, kLustreTestResultError, "-", "section not found", sectionName);
        }
        if (!testNameFound) {
            snprintf(newTestMessage, kLustreTestSysctlMessageSize, "%s:%s:%u:%d:%s:%s - %s", lustre_test_message, "-", 0, kLustreTestResultError, "-", "name not found", testName);
        }
        
        memcpy(lustre_test_message, newTestMessage, kLustreTestSysctlMessageSize);
    } else if (req->newptr) {
        // do nothing
    } else {
        error = SYSCTL_OUT(req, lustre_test_message, kLustreTestSysctlMessageSize);
    }
    
    return error;
}

SYSCTL_DECL(_lustre);
SYSCTL_NODE(, OID_AUTO, lustre, CTLFLAG_RW | CTLFLAG_LOCKED, 0, "lustre");
SYSCTL_PROC(_lustre, OID_AUTO, test, CTLTYPE_STRING|CTLFLAG_RW, lustre_test_message, sizeof(lustre_test_message), lustre_test_sysctl_handler, "A", "Lustre test name to run");

kern_return_t lustre_test_start(kmod_info_t * ki, void * d)
{
    sysctl_register_oid(&sysctl__lustre);
    sysctl_register_oid(&sysctl__lustre_test);
    
    return KERN_SUCCESS;
}

kern_return_t lustre_test_stop(kmod_info_t * ki, void * d)
{
    sysctl_unregister_oid(&sysctl__lustre_test);
    sysctl_unregister_oid(&sysctl__lustre);
    
    return KERN_SUCCESS;
}
