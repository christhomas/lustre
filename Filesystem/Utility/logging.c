//
//  logging.c
//  Filesystem
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

#include "logging.h"

const char * kLFSLoggerBundleId = "com.ciderapps.lustre.Extension";

os_log_t lustre_logger_default;
os_log_t lustre_logger_utility;
os_log_t lustre_logger_vfs;
os_log_t lustre_logger_network;

void lustre_logging_alloc(void)
{
    lustre_logger_default  = os_log_create(kLFSLoggerBundleId, "default");
    lustre_logger_utility  = os_log_create(kLFSLoggerBundleId, "utility");
    lustre_logger_vfs      = os_log_create(kLFSLoggerBundleId, "vfs");
    lustre_logger_network  = os_log_create(kLFSLoggerBundleId, "network");
}

void lustre_logging_free(void)
{
    os_release(lustre_logger_default);
    os_release(lustre_logger_utility);
    os_release(lustre_logger_vfs);
    os_release(lustre_logger_network);
}
