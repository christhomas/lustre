//
//  test.h
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

#ifndef lustre_test_h
#define lustre_test_h

#if DEBUG

#include <mach/mach_types.h>
#include <libkern/libkern.h>
#include <kern/debug.h>

static const uint8_t kLustreTestResultMessageSize   = 200;
static const uint8_t kLustreTestResultFileSize      = 200;

enum lustre_test_result {
    kLustreTestResultSuccess,
    kLustreTestResultFail,
    kLustreTestResultError,
    kLustreTestResultIgnore,
};

#define LUSTRE_TEST_ARGS (enum lustre_test_result * result, char * condition, char * message, char * file, uint32_t * line)
#define LUSTRE_TEST(section, name) \
void lustre_ ## section ## _ ## name ## _test LUSTRE_TEST_ARGS

struct lustre_test_listing {
    const char * section;
    const char * name;
    void (*test)LUSTRE_TEST_ARGS;
};

#define LUSTRE_TEST_IGNORE(m)          \
*result = kLustreTestResultIgnore; \
strlcpy(message, m, kLustreTestResultMessageSize);

#define LUSTRE_ASSERT_XSTR(s) LUSTRE_ASSERT_STR(s)
#define LUSTRE_ASSERT_STR(s) #s

#define LUSTRE_ASSERT_PRIMITIVE(f, l, type, expression, fatal, ...) \
if (strcmp("failure", type) == 0) { \
snprintf(condition, kLustreTestResultMessageSize-1, "%s", type); \
} else { \
snprintf(condition, kLustreTestResultMessageSize-1, "%s %s", type, #expression); \
} \
strlcpy(file, f, kLustreTestResultFileSize); \
*line = l; \
if ((expression) != 1) { \
*result = kLustreTestResultFail; \
bzero(message, kLustreTestResultMessageSize); \
snprintf(message, kLustreTestResultMessageSize-1, __VA_ARGS__); \
if (fatal) {    \
panic(__VA_ARGS__); \
} else { \
return; \
} \
} else { \
*result = kLustreTestResultSuccess; \
snprintf(message, kLustreTestResultMessageSize-1, "success"); \
}

#define LUSTRE_ASSERT(expression) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert", expression, 0, "%d is not true", expression)

#define LUSTRE_ASSERT_FATAL(expression) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert", expression, 1, "%d is not true", expression)

#define LUSTRE_ASSERT_TRUE(value) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert true", (value == 1), 0, "%d is not true", value)

#define LUSTRE_ASSERT_TRUE_FATAL(value) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert true", (value == 1), 1, "%d is not true", value)

#define LUSTRE_ASSERT_FALSE(value) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert false", (value == 0), 0, "%d is not false", value)

#define LUSTRE_ASSERT_FALSE_FATAL(value) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert false", (value == 0), 1, "%d is not false", value)

#define LUSTRE_ASSERT_EQUAL(actual, expected, format) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert equal", (actual == expected), 0, "actual " format " is not equal to " format " expected", actual, expected)

#define LUSTRE_ASSERT_EQUAL_FATAL(actual, expected, format) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert equal", (actual == expected), 1, "actual " format " is not equal to " format " expected", actual, expected)

#define LUSTRE_ASSERT_NOT_EQUAL(actual, expected, format) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert not equal", (actual != expected), 0, "actual " format " is equal to " format " expected", actual, expected)

#define LUSTRE_ASSERT_NOT_EQUAL_FATAL(actual, expected, format) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert not equal", (actual != expected), 1, "actual " format " is equal to " format " expected", actual, expected)

#define LUSTRE_ASSERT_NULL(value) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert null", (value == NULL), 0, "%p is not NULL", value)

#define LUSTRE_ASSERT_NULL_FATAL(value) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert null", (value == NULL), 1, "%p is not NULL", value)

#define LUSTRE_ASSERT_NOT_NULL(value) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert not null", (value != NULL), 0, "%p is NULL", value)

#define LUSTRE_ASSERT_NOT_NULL_FATAL(value) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert not null", (value != NULL), 1, "%p is NULL", value)

#define LUSTRE_ASSERT_STRING_EQUAL(actual, expected) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert string equal", (strcmp(actual, expected) == 0), 0, "actual %s is not equal to %s expected", actual, expected)

#define LUSTRE_ASSERT_STRING_EQUAL_FATAL(actual, expected) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert string equal", (strcmp(actual, expected) == 0), 1, "actual %s is not equal to %s expected", actual, expected)

#define LUSTRE_ASSERT_STRING_NOT_EQUAL(actual, expected) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert string not equal", (strcmp(actual, expected) != 0), 0, "actual %s is equal to %s expected", actual, expected)

#define LUSTRE_ASSERT_STRING_NOT_EQUAL_FATAL(actual, expected) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert string not equal", (strcmp(actual, expected) != 0), 1, "actual %s is equal to %s expected", actual, expected)

#define LUSTRE_ASSERT_NSTRING_EQUAL(actual, expected, count) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert nstring equal", (strncmp(actual, expected, count) == 0), 0, "actual %s is not equal to %s expected for %u characters", actual, expected, count)

#define LUSTRE_ASSERT_NSTRING_EQUAL_FATAL(actual, expected, count) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert nstring equal", (strncmp(actual, expected, count) == 0), 1, "actual %s is not equal to %s expected for %u characters", actual, expected, count)

#define LUSTRE_ASSERT_NSTRING_NOT_EQUAL(actual, expected, count) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert nstring not equal", (strncmp(actual, expected, count) != 0), 0, "actual %s is equal to %s expected for %u characters", actual, expected, count)

#define LUSTRE_ASSERT_NSTRING_NOT_EQUAL_FATAL(actual, expected, count) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "assert nstring not equal", (strncmp(actual, expected, count) != 0), 1, "actual %s is equal to %s expected for %u characters", actual, expected, count)

#define LUSTRE_FAIL(message) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "failure", 1 == 0, 0, message);

#define LUSTRE_FAIL_FATAL(message) \
LUSTRE_ASSERT_PRIMITIVE(__FILE__, __LINE__, "failure", 1 == 0, 1, message);

#endif // DEBUG

#endif /* lustre_test_h */
