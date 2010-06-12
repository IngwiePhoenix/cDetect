/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*************************************************************************
 *
 * http://cdetect.sourceforge.net/
 *
 * $Id: cdetect.c,v 1.71 2007/01/27 16:07:50 breese Exp $
 *
 * Copyright (C) 2005-2010 Bjorn Reese <breese@users.sourceforge.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS AND
 * CONTRIBUTORS ACCEPT NO RESPONSIBILITY IN ANY CONCEIVABLE MANNER.
 *
 ************************************************************************/

#ifndef CDETECT_C_INCLUDE_GUARD
#define CDETECT_C_INCLUDE_GUARD

#define CDETECT_VERSION_MAJOR 0
#define CDETECT_VERSION_MINOR 2
#define CDETECT_VERSION_PATCH 2

/*
  TODO:
    o Alleviate all FIXME comments
    o Document the API
    o How to handle cflags/ldflags in a platform-independent way?
    o Handle two or more mandatory headers or libraries at the same time.
    o Do not use config_ internally
    o Differentiate between shared and local cache? (and hence shared/local
      macros?) Or between generic and project-specific?
    o Only store cdetect_string_t in map? (what about cdetect_option_map?)
    o Only log when option is specified?
    o cdetect_log is not activated during registration
    o config_compile_source() with formatting string
    o Formatting template for substitution instead of cdetect_variable_begin etc.?
    o Regular expressions
    o Use cdetect_file_t more
*/

/** @mainpage

@author Bjorn Reese

@section intro Introduction

cDetect is a tool for detecting platform features, such as the existence
of types, functions, header files, and libraries.

cDetect serves the same purpose as GNU Autoconf, but only requires a C
compiler to work. This makes it directly portable to many non-Unix
platforms.

cDetect is designed to work with any C89 (ANSI X3.159-1989) compiler or
better. A conforming hosted compiler is required to enable the use of
stdio and system().

@section formatting Formatting

@section varsub Variable Substitution

*/

/** @file cdetect.c

@brief Contains all the core functionality to detect operating system
features.

cdetect.c reserves all names starting with config_ and CONFIG_ for public
use, and all names starting with cdetect_ and CDETECT_ for internal use.
*/

/*************************************************************************
 *
 * Preamble
 *
 ************************************************************************/

#define CDETECT_MKVER(v,r,p) (((v) << 24) + ((r) << 16) + (p))

#define CDETECT_VERSION CDETECT_MKVER(CDETECT_VERSION_MAJOR, CDETECT_VERSION_MINOR, CDETECT_VERSION_PATCH)

/*
 * Pre-defined macros.
 *
 * Keep these to a minimum.
 */

#if defined(_MSC_VER)
# define CDETECT_COMPILER_MSC CDETECT_MKVER(_MSC_VER / 100, _MSC_VER % 100, 0)
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
# define CDETECT_OS_WIN32
# define CDETECT_HEADER_WINDOWS_H
# define CDETECT_FUNC__FLUSHALL
#endif

#if defined(unix) || defined(__unix) || defined(__unix__) \
 || defined(_AIX) \
 || (defined(__APPLE__) && defined(__MACH__)) \
 || defined(__Lynx__) \
 || defined(__NetBSD__) \
 || defined(__osf__) \
 || defined(__QNX__) \
 || defined(__CYGWIN__)
# define CDETECT_HEADER_UNISTD_H
#endif

#if defined(CDETECT_HEADER_UNISTD_H)
# include <unistd.h>
# if defined(_XOPEN_XPG3) || (defined(_XOPEN_VERSION) && (_XOPEN_VERSION >= 3))
#  define CDETECT_HEADER_SYS_WAIT_H
# endif
#endif

/*************************************************************************
 *
 * Include files
 *
 ************************************************************************/

#if defined(CDETECT_COMPILER_MSC) && (CDETECT_COMPILER_MSC >= CDETECT_MKVER(14, 0, 0))
/* Avoid security warnings added in Visual Studio 2005 */
# if !defined(_CRT_SECURE_NO_DEPRECATE)
#  define _CRT_SECURE_NO_DEPRECATE 1
# endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#if defined(CDETECT_HEADER_SYS_WAIT_H)
# include <sys/types.h>
# include <sys/wait.h>
#endif
#if defined(CDETECT_HEADER_WINDOWS_H)
# include <windows.h>
#endif

/*
 * Default values of macros
 */

#define CDETECT_CHOST_FILE "cdetect/chost.c"

/*************************************************************************
 *
 * Types
 *
 ************************************************************************/

typedef enum {
    CDETECT_FALSE = 0,
    CDETECT_TRUE = 1
} cdetect_bool_t;

typedef enum {
    CDETECT_REPORT_NULL = 0,
    CDETECT_REPORT_FOUND = 1 << 0,
    CDETECT_REPORT_CACHED = 1 << 1
} cdetect_report_t;

/*
 * Dynamic string
 */

typedef struct cdetect_string
{
    char *content;
    size_t length;
    size_t allocated;
} * cdetect_string_t;

typedef struct cdetect_file
{
    cdetect_string_t path;
} * cdetect_file_t;

/*
 * Fixed array
 */

typedef struct cdetect_array
{
    const char **data;
    int size;
    int count;
} * cdetect_array_t;

/*
 * Double-linked list
 */

typedef struct cdetect_list
{
    struct cdetect_list *next;
    struct cdetect_list *prev;
    void *data;
} * cdetect_list_t;

typedef cdetect_list_t cdetect_stack_t;

/*
 * Map (with ordering though list)
 */

#define CDETECT_MAP_SIZE 101

struct cdetect_map;

typedef struct cdetect_map_element
{
    struct cdetect_map_element *next;
    char *key;
    const void *data;
    struct cdetect_map *parent;
} * cdetect_map_element_t;

typedef void *(*cdetect_map_create_t)(const void *);
typedef void (*cdetect_map_destroy_t)(const void *);

typedef struct cdetect_map {
    /* Member functions */
    cdetect_map_create_t creator;
    cdetect_map_destroy_t destroyer;
    /* Member variables */
    cdetect_list_t first;
    struct cdetect_map_element *chain[CDETECT_MAP_SIZE];
} * cdetect_map_t;

/*
 * Options
 */

typedef cdetect_bool_t (*cdetect_option_callback_t)(const char *, const char *);

typedef struct cdetect_option
{
    char *long_name;
    char *default_value;
    char *default_argument;
    char *help;
    cdetect_option_callback_t callback;
    char short_name;
} * cdetect_option_t;

/*
 * Filters
 */

typedef cdetect_bool_t (*cdetect_tool_check_filter_t)(const char *, void *);
typedef int (*config_tool_check_filter_t)(const char *, void *);

typedef cdetect_bool_t (*cdetect_macro_filter_t)(const char *, const char *);
typedef cdetect_string_t (*cdetect_macro_transform_t)(cdetect_string_t);

/*************************************************************************
 *
 * Data
 *
 ************************************************************************/

/*************************************************************************
 * Global Constants
 */

/* Compiler name */

/* Arguments:
   1 = compiler
   2 = global cflags
   3 = local cflags
   4 = source
   5 = target
   6 = ldflags
*/

static const char *cdetect_compilers_c[][2] = {
    {"cl", "\"%s\" /nologo %s %s %s /Fe%s %s"}, /* Microsoft Visual Studio */
    {"gcc", "%s %s %s %s -o %s %s"}, /* GNU C */
    {"icc", "%s %s %s %s -o %s %s"}, /* Intel C */
    {"xlC_r", "%s %s %s %s -o %s %s"}, /* IBM XL C */
    {"xlC", "%s %s %s %s -o %s %s"}, /* IBM XL C */
    {"cc", "%s %s %s %s -o %s %s"},
    {"c89", "%s %s %s %s -o %s %s"},
    {0, 0}
};

static const char *cdetect_compilers_cxx[][2] = {
    {"cl", "\"%s\" /nologo %s %s %s /Fe%s %s"}, /* Microsoft Visual Studio */
    {"g++", "%s %s %s %s -o %s %s"}, /* GNU C++ */
    {"c++", "%s %s %s %s -o %s %s"}, /* GNU C++ */
    {"icc", "%s %s %s %s -o %s %s"}, /* Intel C++ */
    {"aCC", "%s %s %s %s -o %s %s"}, /* HP aCC */
    {"xlC_r", "%s %s %s %s -o %s %s"}, /* IBM XL C++ */
    {"xlC", "%s %s %s %s -o %s %s"}, /* IBM XL C++ */
    {"CC", "%s %s %s %s -o %s %s"},
    {0, 0}
};

static const char *cdetect_compilers_cpp[][2] = { /* FIXME: arguments */
    {"cl", "/E"},
    {"cpp", ""},
    {"cc", "-E"},
    {0, 0}
};

/* Execution */

const char *cdetect_format_execute = "%s >%s 2>&1"; /* Shell specific */
const char *cdetect_format_remote = "%s %s >%s 2>&1";

/* Compilation */

#if defined(CDETECT_COMPILER_MSC)
const char *cdetect_format_compile = "\"%s\" /nologo %s %s %s /Fe%s %s";
const char *cdetect_format_library = "/DEFAULTLIB:%s";
#else
/* Format: <compiler> <global cflags> <local cflags> <source> -o <target> <ldflags> */
const char *cdetect_format_compile = "%s %s %s %s -o %s %s";
const char *cdetect_format_library = "-l%s";
#endif

/* Files */

#if defined(__cplusplus)
const char *cdetect_suffix_source = ".cpp";
#else
const char *cdetect_suffix_source = ".c";
#endif

#if defined(CDETECT_OS_WIN32)
const char *cdetect_suffix_execute = ".exe";
const char *cdetect_suffix_script = ".bat";
const char *cdetect_file_execute = "cdetmp";
const char cdetect_path_list_separator = ';'; /* Separates paths in the %PATH% environment variable */
const char cdetect_path_separator = '\\'; /* Separates directories in path */
#else
const char *cdetect_suffix_execute = "";
const char *cdetect_suffix_script = ".sh";
const char *cdetect_file_execute = "./cdetmp";
const char cdetect_path_list_separator = ':'; /* Separates paths in the $PATH environment variable */
const char cdetect_path_separator = '/'; /* Separates directories in path */
#endif
const char *cdetect_file_redirection = "cdetmp.txt";

/* Misc */

const char cdetect_map_context_separator = '@';

const char cdetect_cache_separator = '#';
const char cdetect_cache_escape = '\\';
const char *cdetect_cache_identifier_function = "FNC";
const char *cdetect_cache_identifier_header = "HDR";
const char *cdetect_cache_identifier_library = "LIB";
const char *cdetect_cache_identifier_type = "TYP";

const char cdetect_variable_begin = '@';
const char cdetect_variable_end = '@';
const char cdetect_variable_default = '=';
const char cdetect_variable_line = '\n';

const char cdetect_header_separator = ',';
const char cdetect_wildcard_many = '*';
const char cdetect_wildcard_one = '?';

const char *cdetect_null_text = "(null)";

/*************************************************************************
 * Global Variables
 */

cdetect_bool_t cdetect_is_usage = CDETECT_FALSE;
cdetect_bool_t cdetect_is_nested = CDETECT_FALSE;
cdetect_bool_t cdetect_is_silent = CDETECT_FALSE;
cdetect_bool_t cdetect_is_verbose = CDETECT_FALSE;
cdetect_bool_t cdetect_is_dryrun = CDETECT_FALSE;
cdetect_bool_t cdetect_is_compiler_checked = CDETECT_FALSE;
cdetect_bool_t cdetect_is_kernel_checked = CDETECT_FALSE;
cdetect_bool_t cdetect_is_cpu_checked = CDETECT_FALSE;

char *cdetect_command_compile = 0;
char *cdetect_argument_cflags = 0;
char *cdetect_command_remote = 0;

char *cdetect_path = 0;

cdetect_string_t cdetect_compiler_name = 0;
unsigned int cdetect_compiler_version = 0;
cdetect_string_t cdetect_kernel_name = 0;
unsigned int cdetect_kernel_version = 0;
cdetect_string_t cdetect_cpu_name = 0;
unsigned int cdetect_cpu_version = 0;

cdetect_map_t cdetect_option_map = 0;
cdetect_map_t cdetect_option_value_map = 0;

cdetect_map_t cdetect_macro_map = 0;
cdetect_map_t cdetect_function_map = 0;
cdetect_map_t cdetect_header_map = 0;
cdetect_map_t cdetect_type_map = 0;
cdetect_map_t cdetect_library_map = 0;

cdetect_map_t cdetect_tool_map = 0;

cdetect_string_t cdetect_header_format = 0;
cdetect_string_t cdetect_function_format = 0;
cdetect_string_t cdetect_library_format = 0;
cdetect_string_t cdetect_type_format = 0;
cdetect_string_t cdetect_work_directory = 0;
cdetect_string_t cdetect_include_file = 0;
cdetect_file_t cdetect_cache_file = 0;
cdetect_string_t cdetect_copyright_notice = 0;
cdetect_map_t cdetect_build_map = 0;

/*************************************************************************
 *
 * Utility
 *
 ************************************************************************/

/**
   Create normalized version number.

   The format of the version number is 0xVVRRPPPP, where VV = Version,
   RR = Revision, and PPPP = Patch.

   @param major Major version (0 to 255)
   @param minor Minor version (0 to 255)
   @param patch Patch version (0 to 65535)
   @return Normalized version number

   @pre Input parameters must be within the above-mentioned ranges.

   @sa config_compiler_version
   @sa config_kernel_version
   @sa config_cpu_version
*/

unsigned int
config_version(unsigned int major,
               unsigned int minor,
               unsigned int patch)
{
    return CDETECT_MKVER(major, minor, patch);
}

/*
 * Clean up and exit
 */

void cdetect_global_destroy(void); /* Forward declaration */

void
cdetect_exit(int exit_code)
{
    cdetect_global_destroy();
    exit(exit_code);
}

/*
 * Allocate memory
 */

void *
cdetect_allocate(size_t size)
{
    return malloc(size);
}

/*
 * Free memory
 */

void
cdetect_free(void *memory)
{
    if (memory) free(memory);
}

/*
 * Is character a whitespace
 */

int
cdetect_is_space(int c)
{
    return isspace(c);
}

/*
 * Is character an alphanumerical letter
 */

int
cdetect_is_alnum(int c)
{
    return isalnum(c);
}

/*
 * Is character an alphabetic letter
 */

int
cdetect_is_alpha(int c)
{
    return isalpha(c);
}

/*
 * Is character a decimal digit
 */

int
cdetect_is_digit(int c)
{
    return isdigit(c);
}

/*
 * Is character a hexadecimal digit
 */

int
cdetect_is_xdigit(int c)
{
    return isxdigit(c);
}

/*
 * Does string contain character
 */

int
cdetect_is_string_x(char *data, int c)
{
    return (strchr(data, c) != 0);
}

/*
 * Is end of scan group
 */

int
cdetect_is_scangroup_end(int c)
{
    return (c == ']');
}

/*
 * Compare two strings
 */

int
cdetect_strequal(const char *first,
                 const char *second)
{
    if (first == second)
        return 1;

    if ((first == 0) || (second == 0))
        return 0;

    for (;;) {
        if ( (*first == (const char)0) || (*second == (const char)0) )
            break;
        if (toupper(*first) != toupper(*second))
            break;
        ++first;
        ++second;
    }
    return ((*first == (const char)0) && (*second == (const char)0));
}

/**
   Compare if two strings are equal.

   @param first First string.
   @param second Second string.
   @return Boolean indicating whether the two strings are equal or not.

   Case-insensitive comparison.
*/

int config_equal(const char *first,
                 const char *second)
{
    return cdetect_strequal(first, second);
}

/*
 * Compare at most maxlengh characters of two strings
 */

int
cdetect_strequal_max(const char *first,
                     size_t max_length,
                     const char *second)
{
    size_t count;
    cdetect_bool_t atEnd = CDETECT_TRUE;

    assert(first != 0);
    assert(second != 0);

    for (count = 0; count <= max_length; ++count) {
        atEnd = (cdetect_bool_t)( (*first == (const char)0) || (*second == (const char)0) );
        if (atEnd)
            break;
        if (*first != *second)
            break;
        ++first;
        ++second;
    }
    return ( atEnd || (count == max_length) );
}

/*
 * Compare two strings using wildcards
 */

int
cdetect_strmatch(const char *string,
                 const char *pattern)
{
    if (string == pattern)
        return 1;

    if ((string == 0) || (pattern == 0))
        return 0;

    for (; (pattern[0] != cdetect_wildcard_many); ++pattern, ++string) {
        if (string[0] == 0) {
            return (pattern[0] == 0);
        }
        if ( (toupper((int)string[0]) != toupper((int)pattern[0])) &&
             (pattern[0] != cdetect_wildcard_one) ) {
            return 0;
        }
    }
    /* Two-line patch to prevent too much recursion */
    while (pattern[1] == cdetect_wildcard_many)
        pattern++;

    for (; string[0] != 0; ++string) {
        if ( cdetect_strmatch(string, &pattern[1]) ) {
            return 1;
        }
    }

    return 0;
}

/**
   Compare two strings using wildcards.

   @param string String to be searched.
   @param pattern Pattern, including wildcards, to search for.
   @return Boolean value indicating success or failure.

   Case-insensitive comparison.

   The following wildcards can be used
   @li @c * Match any number of characters.
   @li @c ? Match a single character.
*/

int
config_match(const char *string,
             const char *pattern)
{
    return cdetect_strmatch(string, pattern);
}

/*
 * Copy a string
 */

void
cdetect_strcopy(char *target,
                const char *source,
                size_t size)
{
    (void)memcpy(target, source, size);
    target[size] = 0;
}

/*
 * Allocate and copy a string
 */

char *
cdetect_strdup(const char *source)
{
    char *result;
    size_t size;

    if (source == 0)
        return (char *)0;

    size = strlen(source);
    result = (char *)cdetect_allocate(size + 1);
    if (result) {
        cdetect_strcopy(result, source, size);
    }
    return result;
}

/*
 * Skip over characters
 */

size_t
cdetect_strskip(const char *source,
                size_t before,
                int (*is_legal)(int))
{
    size_t after;

    for (after = before; source[after] != 0; ++after) {
        if (!is_legal((int)((unsigned int)source[after])))
            break;
    }
    return after;
}

size_t
cdetect_strskip_x(const char *source,
                  size_t before,
                  int (*is_legal_ex)(char *, int),
                  char *data)
{
    size_t after;

    for (after = before; source[after] != 0; ++after) {
        if (!is_legal_ex(data, (int)((unsigned int)source[after])))
            break;
    }
    return after;
}

/*
 * Skip until characters are found
 */

size_t
cdetect_strskip_until(const char *source,
                      size_t before,
                      int (*is_legal)(int))
{
    size_t after;

    for (after = before; source[after] != 0; ++after) {
        if (is_legal((int)((unsigned char)source[after])))
            break;
    }
    return after;
}

size_t
cdetect_strskip_until_x(const char *source,
                        size_t before,
                        int (*is_legal_ex)(char *, int),
                        char *data)
{
    size_t after;

    for (after = before; source[after] != 0; ++after) {
        if (is_legal_ex(data, (int)((unsigned char)source[after])))
            break;
    }
    return after;
}

/*************************************************************************
 *
 * String
 *
 ************************************************************************/

/*
 * Create a dynamic string
 */

cdetect_string_t
cdetect_string_create(void)
{
    cdetect_string_t self;

    self = (cdetect_string_t)cdetect_allocate(sizeof(*self));
    if (self) {
        self->content = 0;
        self->length = 0;
        self->allocated = 0;
    }
    return self;
}

/*
 * Destroy a dynamic string
 */

void
cdetect_string_destroy(cdetect_string_t self)
{
    if (self) {
        cdetect_free(self->content);
        cdetect_free(self);
    }
}

/*
 * Reserve a certain number of bytes
 */

cdetect_bool_t
cdetect_string_reserve(cdetect_string_t self,
                       size_t size)
{
    char *content;

    assert(self != 0);

    size++; /* Make room for terminating zero */
    if (self->allocated >= size) {
        return CDETECT_TRUE;
    }
    content = (char *)realloc(self->content, size);
    if (content == 0) {
        return CDETECT_FALSE;
    }
    self->content = content;
    self->allocated = size;
    return CDETECT_TRUE;
}

/*
 * Fill string with @p amount occurrences of @p character
 */

cdetect_bool_t
cdetect_string_fill(cdetect_string_t self,
                    char character,
                    size_t amount)
{
    if (cdetect_string_reserve(self, amount)) {
        memset(self->content, character, amount);
        self->length = amount;
        self->content[self->length] = 0;
        return CDETECT_TRUE;
    }
    return CDETECT_FALSE;
}

/*
 * Find first occurrence of a character in a string
 */

size_t
cdetect_string_find_char(cdetect_string_t self,
                         size_t offset,
                         char character)
{
    assert(self != 0);
    assert((self->length == 0) || (offset < self->length));

    for (; offset < self->length; ++offset) {
        if (self->content[offset] == character)
            return offset;
    }
    return self->length;
}

/*
 * Find last occurrence of a character in a string
 */

size_t
cdetect_string_find_last_char(cdetect_string_t self,
                              size_t offset,
                              char character)
{
    size_t current;

    assert(self != 0);
    assert((self->length == 0) || (offset < self->length));

    if (self->length > 0) {
        for (current = self->length - 1; current >= offset; --current) {
            if (self->content[current] == character)
                return current;
        }
    }
    return self->length;
}

/*
 * Report if string contains a @p character
 */

cdetect_bool_t
cdetect_string_contains_char(cdetect_string_t self,
                             char character)
{
    return (cdetect_bool_t)(cdetect_string_find_char(self, 0, character) != self->length);
}

/*
 * Append text to string
 */

cdetect_bool_t
cdetect_string_append(cdetect_string_t self,
                      const char *source)
{
    size_t length;

    assert(self != 0);

    if (source) {
        length = strlen(source);
        if (cdetect_string_reserve(self, self->length + length) == CDETECT_FALSE)
            goto error;
        cdetect_strcopy(&self->content[self->length], source, length);
        self->length += length;
    }
    return CDETECT_TRUE;

 error:
    return CDETECT_FALSE;
}

/*
 * Append a single character to string
 */

cdetect_bool_t
cdetect_string_append_char(cdetect_string_t self,
                           char character)
{
    assert(self != 0);

    if (cdetect_string_reserve(self, self->length + 1) == CDETECT_FALSE)
        return CDETECT_FALSE;

    self->content[self->length++] = character;
    self->content[self->length] = 0; /* Terminating zero */
    return CDETECT_TRUE;
}

/*
 * Append a printable or escaped non-printable character to string
 */

cdetect_bool_t
cdetect_string_append_quoted_char(cdetect_string_t self,
                                  char character)
{
    const char *digits = "0123456789ABCDEF";
    int i = 0;
    int j;
    char buffer[sizeof("\\xFF")];

    assert(self != 0);

    if (isprint(character)) {

        switch (character) {

        case '\"': buffer[i++] = '\\'; buffer[i++] = '\"'; break;
        case '\'': buffer[i++] = '\\'; buffer[i++] = '\''; break;

        default:
            buffer[i++] = character;
            break;
        }

    } else {

        buffer[i++] = '\\';
        switch (character) {

        case '\007': buffer[i++] = 'a'; break;
        case '\b': buffer[i++] = 'b'; break;
        case '\f': buffer[i++] = 'f'; break;
        case '\n': buffer[i++] = 'n'; break;
        case '\r': buffer[i++] = 'r'; break;
        case '\t': buffer[i++] = 't'; break;
        case '\v': buffer[i++] = 'v'; break;

        default:
            buffer[i++] = 'x';
            buffer[i++] = digits[(((int)character) / 16) % 16];
            buffer[i++] = digits[((int)character) % 16];
            break;
        }
    }

    if (cdetect_string_reserve(self, self->length + i) == CDETECT_FALSE)
        return CDETECT_FALSE;

    for (j = 0; j < i; ++j) {
        self->content[self->length++] = buffer[j];
    }
    self->content[self->length] = 0; /* Terminating zero */
    return CDETECT_TRUE;
}

/*
 * Append path (and separator if needed) to string
 */

cdetect_bool_t
cdetect_string_append_path(cdetect_string_t self,
                           const char *path)
{
    cdetect_bool_t success = CDETECT_FALSE;

    if (self->content[self->length] != cdetect_path_separator) {
        success = cdetect_string_append_char(self, cdetect_path_separator);
    }
    if (success) {
        success = cdetect_string_append(self, path);
    }
    return success;
}

/*
 * Convert number to text and append to string
 */

cdetect_bool_t
cdetect_string_append_number(cdetect_string_t self,
                             int number,
                             int base)
{
    const char *digits = "0123456789ABCDEF";
    int absolute_number;
    char buffer[64];
    int i;

    assert(self != 0);
    assert((base > 0) && (base <= (int)strlen(digits)));

    if (number == 0) {
        if (cdetect_string_append_char(self, '0') == CDETECT_FALSE)
            return CDETECT_FALSE;

    } else {
        if (number < 0) {
            absolute_number = -number;
            if (cdetect_string_append_char(self, '-') == CDETECT_FALSE)
                return CDETECT_FALSE;
        } else {
            absolute_number = number;
        }
        i = sizeof(buffer) - 1;
        buffer[i] = 0;
        while ((absolute_number > 0) && (i >= 0)) {
            buffer[--i] = digits[absolute_number % base];
            absolute_number /= base;
        }
        if (cdetect_string_append(self, &buffer[i]) == CDETECT_FALSE)
            return CDETECT_FALSE;
    }
    return CDETECT_TRUE;
}

/*
 * Append partial text to string
 */

cdetect_bool_t
cdetect_string_append_range(cdetect_string_t self,
                            const char *source,
                            size_t first,
                            size_t last)
{
    size_t length;

    assert(self != 0);
    assert(first <= last);

    if ((source != 0) && (first < last)) {
        length = self->length + (last - first);
        if (cdetect_string_reserve(self, length) == CDETECT_FALSE)
            goto error;
        cdetect_strcopy(&self->content[self->length], &source[first], (last - first));
        self->length = length;
    }
    return CDETECT_TRUE;

 error:
    return CDETECT_FALSE;
}

/*
 * Transform string to uppercase
 */

cdetect_string_t
cdetect_string_transform_upper(const char *content)
{
    cdetect_string_t result;
    size_t i;
    char character;

    assert(content != 0);

    result = cdetect_string_create();
    for (i = 0; content[i] != 0; ++i) {
        /*
         * Convert to upper case and replace characters that cannot be used in
         * macro names.
         *
         * See: ISO/IEC 9899:1999 paragraph 6.4.2.1 and 6.10.
         *
         * Caveat: This conversion does not handle macro names that contain
         * universal characters and other implementation-defined characters.
         *
         * Caveat: This conversion does not handle name clashes.
         */

        if (isalnum(content[i])) {
            character = (char)toupper(content[i]);
        } else if (content[i] == '*') {
            character = 'P';
        } else {
            character = '_';
        }

        (void)cdetect_string_append_char(result, character);
    }

    return result;
}

/*
 * Remove all occurrences of any character in @p exclude from string
 */

void
cdetect_string_trim(cdetect_string_t self,
                    const char *exclude)
{
    size_t first;
    size_t last;
    size_t count;

    assert(self != 0);
    assert(exclude != 0);

    first = last = 0;
    while ((count = strcspn(&self->content[last], exclude)) != 0) {
        first = count;
        last += first + strspn(&self->content[first], exclude);
        memmove(&self->content[first], &self->content[last], last - first);
    }
    self->length = first;
    self->content[self->length] = 0;
}

/*
 * All occurrences of @p separator will be replaced with
 * <escape><hex of separator><escape>
 */

cdetect_string_t
cdetect_string_escape(const char *content,
                      const char separator,
                      const char escape)
{
    cdetect_string_t result;
    size_t i;

    assert(content != 0);

    result = cdetect_string_create();
    for (i = 0; content[i] != 0; ++i) {
        if (content[i] == escape) {
            (void)cdetect_string_append_char(result, escape);
            (void)cdetect_string_append_char(result, escape);
        } else if (content[i] == separator) {
            (void)cdetect_string_append_char(result, escape);
            (void)cdetect_string_append_number(result, (int)separator, 16);
            (void)cdetect_string_append_char(result, escape);
        } else {
            (void)cdetect_string_append_char(result, content[i]);
        }
    }
    return result;
}

/*
 * The inverse of cdetect_string_escape
 */

cdetect_string_t
cdetect_string_unescape(const char *content,
                        const char escape)
{
    cdetect_string_t result;
    size_t i;
    size_t after;
    char character;

    assert(content != 0);

    result = cdetect_string_create();
    for (i = 0; content[i] != 0; ++i) {
        if (content[i] == escape) {
            ++i;
            after = cdetect_strskip(content, i, cdetect_is_digit);
            if (after > i) {
                character = (char)strtoul(&content[i], 0, 16);
                (void)cdetect_string_append_char(result, character);
                i = after;
                /* Trailing escape is ignored */
            }
        } else {
            (void)cdetect_string_append_char(result, content[i]);
        }
    }
    return result;
}

cdetect_string_t
cdetect_string_quote(const char *content)
{
    cdetect_string_t result;
    size_t i;

    result = cdetect_string_create();
    if (result) {
        for (i = 0; content[i] != 0; ++i) {
            cdetect_string_append_quoted_char(result, content[i]);
        }
    }
    return result;
}

/*
 * Minimalistic dynamic sprintf-like function that handles cdetect_string_t
 */

cdetect_string_t
cdetect_string_vformat(const char *format,
                       va_list arguments)
{
    cdetect_string_t self;
    int i;
    int start;
    cdetect_bool_t more_modifiers;
    cdetect_bool_t is_size;
    cdetect_bool_t is_alignment;
    cdetect_bool_t is_dynamic;
    cdetect_bool_t is_quote;
    cdetect_bool_t is_alternative;
    cdetect_string_t string;
    char *native;
    const char *append;
    cdetect_string_t append_string;
    int size;

    assert(format != 0);

    self = cdetect_string_create();

    for (i = 0; format[i] != 0; ++i) {

        if (format[i] == '%') {

            /* Modifiers */

            start = i++;

            is_size = CDETECT_FALSE;
            is_alignment = CDETECT_FALSE;
            is_dynamic = CDETECT_FALSE;
            is_quote = CDETECT_FALSE;
            is_alternative = CDETECT_FALSE;
            more_modifiers = CDETECT_TRUE;

            do {

                switch(format[i]) {

                case '*':
                    is_size = CDETECT_TRUE;
                    ++i;
                    break;

                case '-':
                    is_alignment = CDETECT_TRUE;
                    ++i;
                    break;

                case '^':
                    is_dynamic = CDETECT_TRUE;
                    ++i;
                    break;

                case '\'':
                    is_quote = CDETECT_TRUE;
                    ++i;
                    break;

                case '#':
                    is_alternative = CDETECT_TRUE;
                    ++i;
                    break;

                default:
                    more_modifiers = CDETECT_FALSE;
                    break;
                }
            } while (more_modifiers);

            /* Specifiers */

            switch (format[i]) {

            case 's':
                /* String */
                append_string = 0;

                size = is_size ? va_arg(arguments, int) : 0;

                if (is_dynamic) {
                    string = va_arg(arguments, cdetect_string_t);
                    if (string == 0) {
                        append = cdetect_null_text;
                        is_quote = CDETECT_FALSE;
                    } else {
                        append = string->content;
                    }
                } else {
                    native = va_arg(arguments, char *);
                    if (native == 0) {
                        append = cdetect_null_text;
                        is_quote = CDETECT_FALSE;
                    } else {
                        append = native;
                    }
                }

                if (is_alternative) {
                    append_string = cdetect_string_quote(append);
                    append = append_string->content;
                }

                size -= (is_quote ? 2 : 0) + (append ? strlen(append) : 0);

                if (!is_alignment) {
                    for ( ; size > 0; --size) {
                        (void)cdetect_string_append_char(self, ' ');
                    }
                }

                if (is_quote)
                    (void)cdetect_string_append_char(self, '"');
                (void)cdetect_string_append(self, append);
                if (is_quote)
                    (void)cdetect_string_append_char(self, '"');

                if (is_alignment) {
                    for ( ; size > 0; --size) {
                        (void)cdetect_string_append_char(self, ' ');
                    }
                }

                if (is_alternative) {
                    cdetect_string_destroy(append_string);
                }
                break;

            case 'c':
                /* Character */
                assert(!is_alignment);
                assert(!is_dynamic);
                assert(!is_quote);
                assert(!is_size);
                (void)cdetect_string_append_char(self, (char)va_arg(arguments, int));
                break;

            case 'd':
                /* Signed integer */
                assert(!is_alignment);
                assert(!is_dynamic);
                assert(!is_quote);
                assert(!is_size);
                (void)cdetect_string_append_number(self, va_arg(arguments, int), 10);
                break;

            case 'u':
                /* Unsigned integer */
                assert(!is_alignment);
                assert(!is_dynamic);
                assert(!is_quote);
                assert(!is_size);
                (void)cdetect_string_append_number(self, va_arg(arguments, unsigned int), 10);
                break;

            case 'x':
                /* Unsigned hex integer */
                assert(!is_alignment);
                assert(!is_dynamic);
                assert(!is_quote);
                assert(!is_size);
                (void)cdetect_string_append_number(self, va_arg(arguments, unsigned int), 16);
                break;

            case '%':
                /* Escaped % */
                assert(!is_alignment);
                assert(!is_dynamic);
                assert(!is_quote);
                assert(!is_size);
                (void)cdetect_string_append_char(self, format[i]);
                break;

            default:
                for ( ; start <= i; ++start) {
                    (void)cdetect_string_append_char(self, format[start]);
                }
                break;
            }
        } else {
            (void)cdetect_string_append_char(self, format[i]);
        }
    }
    if (self->content == 0) {
        (void)cdetect_string_append(self, "");
    }

    return self;
}

cdetect_string_t
cdetect_string_format(const char *format, ...)
{
    cdetect_string_t self;
    va_list arguments;

    va_start(arguments, format);
    self = cdetect_string_vformat(format, arguments);
    va_end(arguments);

    return self;
}

/*
 * Minimalistic sscanf-like function that handles cdetect_string_t
 */

int
cdetect_string_scan(cdetect_string_t self,
                    const char *format, ...)
{
    va_list arguments;
    int count = 0;
    size_t i;
    size_t j = 0;
    size_t before;
    size_t after;
    cdetect_string_t string;
    cdetect_string_t *string_pointer;
    unsigned int *number_pointer;
    char *char_pointer;
    cdetect_bool_t more_modifiers;
    cdetect_bool_t is_ignore;
    cdetect_bool_t is_dynamic;
    cdetect_bool_t skip;

    assert(format != 0);

    va_start(arguments, format);
    for (i = 0; format[i] != 0; ++i) {

        if (format[i] == '%') {

            /* Modifiers */

            is_ignore = CDETECT_FALSE;
            is_dynamic = CDETECT_FALSE;
            more_modifiers = CDETECT_TRUE;

            do {

                switch(format[i + 1]) {

                case '*':
                    is_ignore = CDETECT_TRUE;
                    ++i;
                    break;

                case '^':
                    is_dynamic = CDETECT_TRUE;
                    ++i;
                    break;

                default:
                    more_modifiers = CDETECT_FALSE;
                    break;
                }
            } while (more_modifiers);

            /* Specifiers */

            switch(format[i + 1]) {

            case '%':
                assert(!is_dynamic);
                if (self->content[j] != format[i]) {
                    count = -1;
                    goto error;
                }
                ++i;
                break;

            case 'n':
                /* Offset */
                assert(!is_dynamic);
                assert(!is_ignore);
                number_pointer = va_arg(arguments, unsigned int *);
                *number_pointer = after;
                break;

            case 'u':
                /* Unsigned integer */
                assert(!is_dynamic);
                before = cdetect_strskip(self->content, j, cdetect_is_space);
                after = cdetect_strskip(self->content, before, cdetect_is_digit);
                if (after > before) {
                    if (!is_ignore) {
                        number_pointer = va_arg(arguments, unsigned int *);
                        *number_pointer = (unsigned int)strtoul(&self->content[before], 0, 10);
                    }
                    j = after;
                }
                ++count;
                ++i;
                break;

            case 'x':
                /* Unsigned hex integer */
                assert(!is_dynamic);
                before = cdetect_strskip(self->content, j, cdetect_is_space);
                /* FIXME: Skip 0x if present */
                after = cdetect_strskip(self->content, before, cdetect_is_xdigit);
                if (after > before) {
                    if (!is_ignore) {
                        number_pointer = va_arg(arguments, unsigned int *);
                        *number_pointer = (unsigned int)strtoul(&self->content[before], 0, 16);
                    }
                }
                j = after;
                ++count;
                ++i;
                break;

            case 's':
                /* String */
                before = cdetect_strskip(self->content, j, cdetect_is_space);
                after = cdetect_strskip_until(self->content, before, cdetect_is_space);

                if (after > before) {
                    if (!is_ignore) {
                        if (is_dynamic) {
                            string_pointer = va_arg(arguments, cdetect_string_t *);
                            *string_pointer = cdetect_string_create();
                            for (; after > before; ++before) {
                                (void)cdetect_string_append_char(*string_pointer, self->content[before]);
                            }
                        } else {
                            char_pointer = va_arg(arguments, char *);
                            cdetect_strcopy(char_pointer, &self->content[before], after - before);
                        }
                    }
                }
                j = after;
                ++count;
                ++i;
                break;

            case '[':
                /* Character group */
                ++i;
                string = cdetect_string_create();
                skip = (format[i + 1] == '^') ? CDETECT_TRUE : CDETECT_FALSE;
                /* Collect characters until ']' */
                before = i + (skip ? 2 : 1);
                after = cdetect_strskip_until(format, before, cdetect_is_scangroup_end);
                (void)cdetect_string_append_range(string, format, before, after);
                i = after;
                /* Read or skip any of these characters */
                before = j;
                after = skip
                    ? cdetect_strskip_until_x(self->content, before, cdetect_is_string_x, string->content)
                    : cdetect_strskip_x(self->content, before, cdetect_is_string_x, string->content);
                if (after >= before) {
                    if (!is_ignore) {
                        if (is_dynamic) {
                            string_pointer = va_arg(arguments, cdetect_string_t *);
                            *string_pointer = cdetect_string_create();
                            (void)cdetect_string_append_range(*string_pointer, self->content, before, after);
                        } else {
                            char_pointer = va_arg(arguments, char *);
                            cdetect_strcopy(char_pointer, &self->content[before], after - before);
                        }
                    }
                }
                j = after;
                cdetect_string_destroy(string);
                ++count;
                break;

            default:
                /* Unknown format */
                assert(0);
                break;
            }
        } else {
            /* Match character */
            if (self->content[j] != format[i]) {
                count = -1;
                goto error;
            }
            ++j;
        }
        if (j >= self->length)
            break;
    }
 error:
    va_end(arguments);
    return count;
}

/*
 * Split string into two at the separator
 */

cdetect_string_t
cdetect_string_split(cdetect_string_t self,
                     const char separator)
{
    size_t i;
    cdetect_string_t result = 0;

    assert(self != 0);

    i = cdetect_string_find_char(self, 0, separator);
    if (i != self->length) {
        self->content[i] = 0;
        self->length = i;
        result = cdetect_string_format("%s", &(self->content[i + 1]));
    }

    return result;
}

/*************************************************************************
 *
 * Fixed string array
 *
 ************************************************************************/

/*
 * Create array wrapper
 */

cdetect_array_t
cdetect_array_create(const char **data, int size)
{
    cdetect_array_t self;

    self = (cdetect_array_t)cdetect_allocate(sizeof(*self));
    if (self) {
        self->data = data;
        self->size = size;
        self->count = 0;
    }
    return self;
}

/*
 * Destroy array wrapper
 */

void
cdetect_array_destroy(cdetect_array_t self)
{
    cdetect_free(self);
}

/*
 * Return current instance and advance pointer to next
 */

const char *
cdetect_array_next(cdetect_array_t self)
{
    assert(self != 0);

    if (self->count >= self->size)
        return 0;
    if (self->count < 0)
        self->count = 0;

    return self->data[self->count++];
}

const char *
cdetect_array_former(cdetect_array_t self)
{
    assert(self != 0);

    if (self->count < 0)
        return 0;
    if (self->count > self->size)
        self->count = self->size;

    return self->data[--self->count];
}

const char *
cdetect_array_first(cdetect_array_t self)
{
    assert(self != 0);

    self->count = 0;

    return cdetect_array_next(self);
}

/*************************************************************************
 *
 * List
 *
 ************************************************************************/

/*
 * Create a single list element
 */

cdetect_list_t
cdetect_list_element_create(void *data)
{
    cdetect_list_t self;

    self = (cdetect_list_t)cdetect_allocate(sizeof(*self));
    if (self) {
        self->next = 0;
        self->prev = 0;
        self->data = data;
    }
    return self;
}

/*
 * Destroy a single list element
 */

void
cdetect_list_element_destroy(cdetect_list_t self)
{
    cdetect_free(self);
}

/*
 * Create a list
 */

cdetect_list_t
cdetect_list_create(void)
{
    /* Create sentinel */
    return cdetect_list_element_create(0);
}

/*
 * Destroy a list
 */

void
cdetect_list_clear(cdetect_list_t self)
{
    cdetect_list_t current;
    cdetect_list_t next;

    assert(self != 0);

    current = self->next; /* Skip sentinel */
    while (current) {
        next = current->next;
        cdetect_list_element_destroy(current);
        current = next;
    }
    self->next = 0;
}

void
cdetect_list_destroy(cdetect_list_t self)
{
    if (self) {
        cdetect_list_clear(self);
        cdetect_list_element_destroy(self);
    }
}

/*
 * Insert data after current location
 */

void
cdetect_list_insert(cdetect_list_t self,
                    void *data)
{
    cdetect_list_t current;
    cdetect_list_t next;

    assert(self != 0);

    current = cdetect_list_element_create(data);
    next = self->next;
    current->next = next;
    current->prev = self;
    self->next = current;
}

/*
 * Append data at end of list
 */

void
cdetect_list_append(cdetect_list_t self,
                    void *data)
{
    cdetect_list_t last;

    assert(self != 0);

    if (self->next) {
        /* FIXME: use sentinel->prev as last */
        for (last = self; last->next; last = last->next)
            continue;
    } else {
        last = self;
    }
    cdetect_list_insert(last, data);
}


cdetect_list_t
cdetect_list_find(cdetect_list_t self,
                  void *data)
{
    cdetect_list_t current;

    assert(self != 0);
    assert(data != 0);

    for (current = self->next; current; current = current->next) {
        if (current->data == data) {
            return current;
        }
    }
    return 0;
}

void
cdetect_list_remove(cdetect_list_t self,
                    void *data)
{
    cdetect_list_t current;

    assert(self != 0);

    current = cdetect_list_find(self, data);
    if (current) {
        if (current->next)
            current->next->prev = current->prev;
        if (current->prev)
            current->prev->next = current->next;
        cdetect_list_element_destroy(current);
    }
}

void *
cdetect_list_front(cdetect_list_t self)
{
    assert(self != 0);

    return (self->data == 0) ? self->next : 0;
}

void *
cdetect_list_back(cdetect_list_t self)
{
    cdetect_list_t current;

    assert(self != 0);

    for (current = self->next;
         current && current->next;
         current = current->next)
        continue;
    return current;
}

void *
cdetect_list_next(cdetect_list_t self)
{
    assert(self != 0);

    return self->next;
}

void
cdetect_list_splice(cdetect_list_t self,
                    cdetect_list_t other)
{
    cdetect_list_t current;
    cdetect_list_t next;

    current = cdetect_list_back(self);
    if (current) {
        next = cdetect_list_front(other);
        if (next) {
            current->next = next;
            next->prev = current;
        }
        other->next = 0;
    }
}

void *
cdetect_list_first(cdetect_list_t self)
{
    assert(self != 0);

    return (self->next == 0) ? 0 : self->next->data;
}

cdetect_bool_t
cdetect_list_empty(cdetect_list_t self)
{
    assert(self != 0);

    return (cdetect_list_front(self) == 0);
}

cdetect_stack_t
cdetect_stack_create(void)
{
    return cdetect_list_create();
}

void
cdetect_stack_destroy(cdetect_stack_t self)
{
    cdetect_list_destroy(self);
}

cdetect_bool_t
cdetect_stack_empty(cdetect_list_t self)
{
    return cdetect_list_empty(self);
}

void *
cdetect_stack_top(cdetect_stack_t self)
{
    return cdetect_list_first(self);
}

void
cdetect_stack_push(cdetect_stack_t self,
                   void *data)
{
    cdetect_list_insert(self, data);
}

void *
cdetect_stack_pop(cdetect_stack_t self)
{
    void *result;

    result = cdetect_stack_top(self);
    cdetect_list_remove(self, result);
    return result;
}

/*************************************************************************
 *
 * Map
 *
 ************************************************************************/

/*
 * Create a single map element
 */

cdetect_map_element_t
cdetect_map_element_create(cdetect_map_t parent)
{
    cdetect_map_element_t self;

    self = (cdetect_map_element_t)cdetect_allocate(sizeof(*self));
    if (self) {
        self->next = 0;
        self->key = 0;
        self->data = 0;
        self->parent = parent;
    }
    return self;
}

/*
 * Destroy a single map element
 */

void
cdetect_map_element_destroy(cdetect_map_element_t self)
{
    if (self) {
        cdetect_free(self->key);
        if (self->parent->destroyer) self->parent->destroyer(self->data);
        cdetect_free(self);
    }
}

/*
 * Destroy all map elements in a single chain
 */

void
cdetect_map_element_destroy_chain(cdetect_map_element_t self)
{
    cdetect_map_element_t next;

    while (self) {
        next = self->next;
        cdetect_map_element_destroy(self);
        self = next;
    }
}


/*
 * Set value of element
 */

void
cdetect_map_element_value(cdetect_map_element_t self,
                          const void *data)
{
    /* Destroy old value */
    if ((self->parent->destroyer != 0) && (self->data != 0)) {
        self->parent->destroyer(self->data);
    }
    /* Assign new value */
    self->data = (self->parent->creator != 0)
        ? self->parent->creator(data)
        : data;
}

/*
 * Find element in given chain
 */

cdetect_map_element_t
cdetect_map_element_find(cdetect_map_element_t self,
                         const char *key)
{
    cdetect_map_element_t element;

    for (element = self; element != 0; element = element->next) {
        if (cdetect_strequal(key, element->key))
            break;
    }
    return element;
}

/*
 * Create map
 */

cdetect_map_t
cdetect_map_create(cdetect_map_create_t creator,
                   cdetect_map_destroy_t destroyer)
{
    cdetect_map_t self;
    unsigned int i;

    self = (cdetect_map_t)cdetect_allocate(sizeof(*self));
    if (self) {
        self->creator = creator;
        self->destroyer = destroyer;
        self->first = cdetect_list_create();
        for (i = 0; i < CDETECT_MAP_SIZE; ++i) {
            self->chain[i] = 0;
        }
    }
    return self;
}

/*
 * Destroy map
 */

void
cdetect_map_destroy(cdetect_map_t self)
{
    unsigned int i;

    if (self) {
        for (i = 0; i < CDETECT_MAP_SIZE; ++i) {
            cdetect_map_element_destroy_chain(self->chain[i]);
        }
        cdetect_list_destroy(self->first);
        cdetect_free(self);
    }
}

/*
 * Calculate a hash value as index
 */

unsigned int
cdetect_map_index(const char *key)
{
    unsigned int hash = 0;
    char character;

    assert(key != 0);

    while ( (character = *key++) != (char)0) {
        hash *= 31;
        hash += (unsigned int)((unsigned char)character);
    }
    return (hash % CDETECT_MAP_SIZE);
}

/*
 * Store a key/value pair for later use
 */

cdetect_map_element_t
cdetect_map_remember(cdetect_map_t self,
                     const char *key,
                     const void *data)
{
    unsigned int i;
    cdetect_map_element_t result;

    assert(self != 0);
    assert(key != 0);
    /* value can be null pointer */

    i = cdetect_map_index(key);
    result = cdetect_map_element_find(self->chain[i], key);
    if (result == 0) {
        /* Create new entry */
        result = cdetect_map_element_create(self);
        result->key = cdetect_strdup(key);
        result->next = self->chain[i];
        self->chain[i] = result;
        cdetect_list_append(self->first, result);
    }
    cdetect_map_element_value(result, data);
    return result;
}

/*
 * Find the value of a given key
 */

cdetect_map_element_t
cdetect_map_lookup(cdetect_map_t self,
                   const char *key)
{
    assert(self != 0);
    assert(key != 0);

    return cdetect_map_element_find(self->chain[cdetect_map_index(key)], key);
}

/*
 * Store key/value pair for later use
 */

cdetect_map_element_t
cdetect_map_remember_context(cdetect_map_t self,
                             const char *context,
                             const char *key,
                             const void *data)
{
    cdetect_map_element_t result;
    cdetect_string_t full_key;

    assert(self != 0);
    assert(key != 0);

    if ((context == 0) || (context[0] == 0)) {
        full_key = cdetect_string_format("%s", key);
    } else {
        full_key = cdetect_string_format("%s%c%s",
                                         context,
                                         cdetect_map_context_separator,
                                         key);
    }

    result = cdetect_map_remember(self, full_key->content, data);

    cdetect_string_destroy(full_key);

    return result;
}

/*
 * Find element of a given key
 */

cdetect_map_element_t
cdetect_map_lookup_context(cdetect_map_t self,
                           const char *context,
                           const char *key)
{
    cdetect_map_element_t result;
    cdetect_string_t full_key;

    assert(self != 0);
    assert(key != 0);

    if ((context == 0) || (context[0] == 0)) {
        full_key = cdetect_string_format("%s", key);
    } else {
        full_key = cdetect_string_format("%s%c%s",
                                         context,
                                         cdetect_map_context_separator,
                                         key);
    }
    result = cdetect_map_lookup(self, full_key->content);

    cdetect_string_destroy(full_key);

    return result;
}

/*************************************************************************
 *
 * Regular Expressions (using NFA simulation)
 *
 ************************************************************************/

typedef struct cdetect_automata_state
{
    cdetect_list_t outgoing; /* transitions */
} * cdetect_automata_state_t;

/* FIXME: combine automat and transitions? */

typedef enum {
    CDETECT_TRANSITION_AUTOMAT,
    CDETECT_TRANSITION_LAMBDA,
    CDETECT_TRANSITION_ANY,
    CDETECT_TRANSITION_LITERAL,
    CDETECT_TRANSITION_DIGIT,
    CDETECT_TRANSITION_SPACE,
    CDETECT_TRANSITION_WORD
} cdetect_transition_type_t;

typedef struct cdetect_automata_transition
{
    cdetect_automata_state_t begin;
    cdetect_automata_state_t end;
    cdetect_transition_type_t type;
    char letter; /* Only for literal type */
} * cdetect_automata_transition_t;

typedef struct cdetect_automat
{
    cdetect_automata_state_t start;
    cdetect_automata_state_t final;
} * cdetect_automata_t;

typedef struct cdetect_regexp
{
    cdetect_list_t present; /* states */
    cdetect_list_t future; /* states */
    cdetect_automata_t automat;
    /* Parsing */
    const char *format;
} * cdetect_regexp_t;

cdetect_automata_state_t
cdetect_automata_state_create(void)
{
    cdetect_automata_state_t self;

    self = (cdetect_automata_state_t)cdetect_allocate(sizeof(*self));
    if (self) {
        self->outgoing = cdetect_list_create();
    }
    return self;
}

void
cdetect_automata_state_destroy(cdetect_automata_state_t self)
{
    if (self) {
        cdetect_list_destroy(self->outgoing);
    }
    cdetect_free(self);
}

cdetect_automata_transition_t
cdetect_automata_transition_create(void)
{
    cdetect_automata_transition_t self;

    self = (cdetect_automata_transition_t)cdetect_allocate(sizeof(*self));
    if (self) {
        self->begin = 0;
        self->end = 0;
        self->type = CDETECT_TRANSITION_AUTOMAT;
        self->letter = 0;
    }
    return self;
}

void
cdetect_automata_transition_destroy(cdetect_automata_transition_t self)
{
    cdetect_free(self);
}

void
cdetect_automata_state_connect(cdetect_automata_state_t self,
                              cdetect_automata_state_t other,
                              cdetect_transition_type_t type)
{
    cdetect_automata_transition_t transition;

    assert(self != 0);

    transition = cdetect_automata_transition_create();
    if (transition) {
        transition->begin = self;
        transition->end = other;
        transition->type = type;
        cdetect_list_append(self->outgoing, transition);
    }
}

void
cdetect_automata_state_connect_literal(cdetect_automata_state_t self,
                                      cdetect_automata_state_t other,
                                      char letter)
{
    cdetect_automata_transition_t transition;

    assert(self != 0);

    transition = cdetect_automata_transition_create();
    if (transition) {
        transition->begin = self;
        transition->end = other;
        transition->type = CDETECT_TRANSITION_LITERAL;
        transition->letter = letter;
        cdetect_list_append(self->outgoing, transition);
    }
}

cdetect_automata_t
cdetect_automata_create(void)
{
    cdetect_automata_t self;

    self = (cdetect_automata_t)cdetect_allocate(sizeof(*self));
    if (self) {
        self->start = cdetect_automata_state_create();
        self->final = cdetect_automata_state_create();
    }
    return self;
}

void
cdetect_automata_destroy(cdetect_automata_t self)
{
    if (self) {
        cdetect_automata_state_destroy(self->start);
        cdetect_automata_state_destroy(self->final);
        cdetect_free(self);
    }
}

cdetect_regexp_t
cdetect_regexp_create(void)
{
    cdetect_regexp_t self;

    self = (cdetect_regexp_t)cdetect_allocate(sizeof(*self));
    if (self) {
        self->present = cdetect_list_create();
        self->future = cdetect_list_create();
        self->automat = cdetect_automata_create();
    }
    return self;
}

void
cdetect_regexp_destroy(cdetect_regexp_t self)
{
    if (self) {
        cdetect_list_destroy(self->present);
        cdetect_list_destroy(self->future);
        cdetect_automata_destroy(self->automat);
        cdetect_free(self);
    }
}

cdetect_automata_state_t
cdetect_regexp_parse_class(cdetect_regexp_t self,
                           cdetect_automata_state_t begin)
{
    cdetect_automata_state_t end = 0;

    assert(self != 0);
    assert(self->format != 0);

    switch (self->format[1]) {

    case '\\':
        end = cdetect_automata_state_create();
        cdetect_automata_state_connect_literal(begin, end, self->format[1]);
        break;

    case 'd':
        end = cdetect_automata_state_create();
        cdetect_automata_state_connect(begin, end, CDETECT_TRANSITION_DIGIT);
        break;

    case 's':
        end = cdetect_automata_state_create();
        cdetect_automata_state_connect(begin, end, CDETECT_TRANSITION_SPACE);
        break;

    case 'w':
        end = cdetect_automata_state_create();
        cdetect_automata_state_connect(begin, end, CDETECT_TRANSITION_WORD);
        break;

    default:
        assert(0);
        break;
    }

    return end;
}

void
cdetect_regexp_parse(cdetect_regexp_t self,
                     cdetect_automata_t automat)
{
    cdetect_bool_t keep_parsing;
    /* FIXME: Can current* and last* be combined (and one removed)? */
    cdetect_automata_state_t current_begin;
    cdetect_automata_state_t current_end;
    cdetect_automata_state_t alternation_begin;
    cdetect_automata_state_t last_begin = 0;
    cdetect_automata_state_t last_end = 0;
    cdetect_automata_t group;
    char letter = 0;

    current_begin = automat->start;
    alternation_begin = current_begin;

    for (keep_parsing = CDETECT_TRUE; keep_parsing; self->format++) {

        switch (self->format[0]) {

        case '?': /* Zero or one */

            if (last_begin && last_end) {
                cdetect_automata_state_connect(last_begin,
                                              last_end,
                                              CDETECT_TRANSITION_LAMBDA);
            }
            break;

        case '*': /* Zero or more */

            if (last_begin && last_end) {
                cdetect_automata_state_connect(last_begin,
                                              last_end,
                                              CDETECT_TRANSITION_LAMBDA);
                if (letter == 0) {
                    cdetect_automata_state_connect(last_end,
                                                  last_end,
                                                  CDETECT_TRANSITION_LAMBDA);
                } else {
                    cdetect_automata_state_connect_literal(last_end,
                                                          last_end,
                                                          letter);
                }
            }
            break;

        case '+': /* One or more */

            if (last_begin && last_end) {
                cdetect_automata_state_connect(last_end,
                                              last_begin,
                                              CDETECT_TRANSITION_LAMBDA);
            }
            break;

        case '|': /* Alternation */

            cdetect_automata_state_connect(last_end,
                                          automat->final,
                                          CDETECT_TRANSITION_LAMBDA);
            current_begin = alternation_begin;
            last_begin = last_end = 0;
            break;

        case '(': /* Group beginning */

            /* Recurse */
            group = cdetect_automata_create();
            self->format++;
            cdetect_regexp_parse(self, group);
            self->format--;
            /* Merge automats */
            if (last_end != 0) {
                cdetect_automata_state_connect(last_end,
                                              group->start,
                                              CDETECT_TRANSITION_LAMBDA);
            }
            current_begin = cdetect_automata_state_create();
            cdetect_automata_state_connect(group->final,
                                          current_begin,
                                          CDETECT_TRANSITION_LAMBDA);
            last_begin = last_end;
            last_end = group->final;
            letter = 0;
            /* Release automat */
            group->start = 0;
            group->final = 0;
            cdetect_automata_destroy(group);
            break;

        case ')': /* Group ending */

            keep_parsing = CDETECT_FALSE;
            break;

        case '\\': /* Escape */

            current_end = cdetect_regexp_parse_class(self, current_begin);
            last_begin = current_begin;
            last_end = current_end;
            current_begin = current_end;
            self->format++;
            break;

        case '.': /* Any letter */

            current_end = cdetect_automata_state_create();
            cdetect_automata_state_connect_literal(current_begin,
                                                  current_end,
                                                  CDETECT_TRANSITION_ANY);
            last_begin = current_begin;
            last_end = current_end;
            current_begin = current_end;
            break;

        case 0: /* End of string */

            keep_parsing = CDETECT_FALSE;
            break;

        default: /* Literal */

            letter = self->format[0];
            current_end = cdetect_automata_state_create();
            cdetect_automata_state_connect_literal(current_begin,
                                                  current_end,
                                                  letter);
            last_begin = current_begin;
            last_end = current_end;
            current_begin = current_end;
            break;
        }
    }
    cdetect_automata_state_connect(last_end,
                                  automat->final,
                                  CDETECT_TRANSITION_LAMBDA);
}

cdetect_regexp_t
cdetect_regexp_compile(const char *format)
{
    cdetect_regexp_t self;

    self = cdetect_regexp_create();
    if (self) {

        self->format = format;
        cdetect_regexp_parse(self, self->automat);

    }
    return self;
}

/*
 * Calculate states reachable through lambda transitions (lambda closure)
 */

void
cdetect_regexp_reach(cdetect_regexp_t self)
{
    cdetect_stack_t stack;
    cdetect_list_t current_state;
    cdetect_list_t current_transition;
    cdetect_automata_state_t state;
    cdetect_automata_transition_t transition;

    (void)self;

    stack = cdetect_stack_create();
    if (stack) {

        for (current_state = cdetect_list_front(self->present);
             current_state != 0;
             current_state = cdetect_list_next(current_state)) {

            cdetect_stack_push(stack, current_state->data);
        }

        while (!cdetect_stack_empty(stack)) {

            state = (cdetect_automata_state_t)cdetect_stack_pop(stack);
            assert(state != 0);

            for (current_transition = cdetect_list_front(state->outgoing);
                 current_transition != 0;
                 current_transition = cdetect_list_next(current_transition)) {

                transition = (cdetect_automata_transition_t)current_transition->data;
                if (transition) {
                    if (transition->type == CDETECT_TRANSITION_LAMBDA) {
                        if (cdetect_list_find(self->present, transition->end) == 0) {
                            cdetect_list_append(self->present, transition->end);
                            cdetect_stack_push(stack, transition->end);
                        }
                    }
                }
            }
        }
        cdetect_stack_destroy(stack);
    }
}

void
cdetect_regexp_step(cdetect_regexp_t self,
                    char input)
{
    cdetect_list_t current_state;
    cdetect_list_t current_transition;
    cdetect_automata_state_t state;
    cdetect_automata_transition_t transition;

    for (current_state = cdetect_list_front(self->present);
         current_state != 0;
         current_state = cdetect_list_next(current_state)) {

        state = (cdetect_automata_state_t)current_state->data;

        for (current_transition = cdetect_list_front(state->outgoing);
             current_transition != 0;
             current_transition = cdetect_list_next(current_transition)) {

            transition = (cdetect_automata_transition_t)current_transition->data;
            if (transition) {

                switch (transition->type) {

                case CDETECT_TRANSITION_ANY:
                    cdetect_list_append(self->future, transition->end);
                    break;

                case CDETECT_TRANSITION_LITERAL:
                    if (transition->letter == input) {
                        cdetect_list_append(self->future, transition->end);
                    }
                    break;

                case CDETECT_TRANSITION_DIGIT:
                    if (cdetect_is_digit(input)) {
                        cdetect_list_append(self->future, transition->end);
                    }
                    break;

                case CDETECT_TRANSITION_SPACE:
                    if (cdetect_is_space(input)) {
                        cdetect_list_append(self->future, transition->end);
                    }
                    break;

                case CDETECT_TRANSITION_WORD:
                    if (cdetect_is_alnum(input)) {
                        cdetect_list_append(self->future, transition->end);
                    }
                    break;

                case CDETECT_TRANSITION_LAMBDA:
                    /* FIXME: lambda transitions ought the be removed */
                    break;

                default:
                    assert(0);
                    break;
                }
            }
        }
    }
}

cdetect_bool_t
cdetect_regexp_match(cdetect_regexp_t self,
                     const char *input)
{
    int i;
    cdetect_list_t swapper;

    if ((self == 0) || (self->automat == 0))
        return CDETECT_FALSE;

    cdetect_list_clear(self->present);
    cdetect_list_append(self->present, self->automat->start);

    for (i = 0; input[i] != 0; ++i) {

        cdetect_list_clear(self->future);

        cdetect_regexp_reach(self);
        cdetect_regexp_step(self, input[i]);

        swapper = self->present;
        self->present = self->future;
        self->future = swapper;

        if (cdetect_list_empty(self->present)) {
            return CDETECT_FALSE;
        }
    }
    /* FIXME: check for final state */
    /* return (cdetect_list_first(self->present) == self->automat->final); */
    return CDETECT_TRUE;
}

/*************************************************************************
 *
 * File
 *
 ************************************************************************/

cdetect_file_t
cdetect_file_create(void)
{
    cdetect_file_t self;

    self = (cdetect_file_t)cdetect_allocate(sizeof(*self));
    if (self) {
        self->path = cdetect_string_create();
    }
    return self;
}

void
cdetect_file_destroy(cdetect_file_t self)
{
    if (self) {
        cdetect_string_destroy(self->path);
        cdetect_free(self);
    }
}

void
cdetect_file_append(cdetect_file_t self, const char *path)
{
    if (self->path->content == 0) {
        cdetect_string_append(self->path, path);
    } else {
        cdetect_string_append_path(self->path, path);
    }
}

/*
 * Check if file exists
 */

cdetect_bool_t
cdetect_file_exist(const char *filename)
{
    cdetect_bool_t success = CDETECT_FALSE;
    FILE *file;

    if (filename) {
        file = fopen(filename, "r");
        if (file) {
            success = CDETECT_TRUE;
            (void)fclose(file);
        }
    }
    return success;
}

/*
 * Determine unique file name (like tmpnam or mktemp)
 *
 * Given file[.suffix] this will create the name file<hexnum>[.suffix]
 */

cdetect_string_t
cdetect_file_unique_name(const char *name)
{
    cdetect_string_t filename;
    size_t offset;
    int count;
    cdetect_string_t result;

    filename = cdetect_string_format("%s", name);

    offset = cdetect_string_find_last_char(filename, 0, '.');
    for (count = 0; count < 256; ++count) {
        result = cdetect_string_create();
        (void)cdetect_string_append_range(result, filename->content, 0, offset);
        (void)cdetect_string_append_number(result, count, 16);
        (void)cdetect_string_append_range(result, filename->content, offset, filename->length);
        if (!cdetect_file_exist(result->content))
            break;
        cdetect_string_destroy(result);
        result = 0;
    }
    cdetect_string_destroy(filename);

    return result;
}

/*
 * Delete a file on the file system
 */

cdetect_bool_t
cdetect_file_remove(const char *filename)
{
    assert(filename != 0);

    return (cdetect_bool_t)remove(filename);
}

/*
 * Determine the file size
 */

long
cdetect_file_size(const char *filename)
{
    long result = -1;
    FILE *file;

    assert(filename != 0);

    file = fopen(filename, "r");
    if (file) {

        if (fseek(file, 0, SEEK_END) != -1) {

            result = ftell(file);
        }
        (void)fclose(file);
    }
    return result;
}

/*
 * Read file content into string
 */

cdetect_bool_t
cdetect_file_read(const char *filename,
                  cdetect_string_t *data)
{
    int success = 0;
    FILE *file;
    long size;

    assert(filename != 0);
    assert(data != 0);

    size = cdetect_file_size(filename);
    if (size != -1) {
        file = fopen(filename, "r");
        if (file) {
            *data = cdetect_string_create();
            if ( (*data != 0) &&
                 (cdetect_string_reserve(*data, (size_t)size + 1) == CDETECT_TRUE) ) {

                (*data)->length = fread((*data)->content, 1, (*data)->allocated, file);
                (*data)->content[(*data)->length] = (char)0;
                success = ((*data)->length <= (size_t)size);
            }
            (void)fclose(file);
        }
    }
    return (cdetect_bool_t)success;
}

/*
 * Write string to file
 */

cdetect_bool_t
cdetect_file_write(const char *filename,
                   const char *mode,
                   cdetect_string_t data)
{
    int success = 0;
    FILE *file;

    assert(filename != 0);
    assert(data != 0);

    file = fopen(filename, mode);
    if (file) {
        if (fwrite(data->content, 1, data->length, file) == data->length) {
            success = 1;
        }
        (void)fclose(file);
    }
    return (cdetect_bool_t)success;
}

/*
 * Overwrite file with string
 */

cdetect_bool_t
cdetect_file_overwrite(const char *filename,
                       cdetect_string_t data)
{
    return cdetect_file_write(filename, "w", data);
}

/*
 * Append string to file
 */

cdetect_bool_t
cdetect_file_write_after(cdetect_string_t filename,
                         const char *format, ...)
{
    cdetect_bool_t success = CDETECT_FALSE;
    va_list arguments;
    cdetect_string_t data;

    if (filename == 0)
        return success;

    va_start(arguments, format);
    data = cdetect_string_vformat(format, arguments);
    va_end(arguments);
    success = cdetect_file_write(filename->content, "a", data);
    cdetect_string_destroy(data);

    return (cdetect_bool_t)success;
}

#if 0 /* FIXME */
cdetect_bool_t
cdetect_work_file_append(cdetect_string_t filename,
                         const char *format, ...)
{
    cdetect_bool_t success = CDETECT_FALSE;
    cdetect_string_t workpath;
    va_list arguments;
    cdetect_string_t data;

    if (filename == 0)
        return success;

    workpath = cdetect_string_format("%^s", cdetect_work_directory);
    cdetect_string_append_path(workpath, filename);

    va_start(arguments, format);
    data = cdetect_string_vformat(format, arguments);
    va_end(arguments);
    success = cdetect_file_write(workpath->content, "a", data);
    cdetect_string_destroy(data);
    cdetect_string_destroy(workpath);

    return (cdetect_bool_t)success;
}
#endif

/*************************************************************************
 *
 * Output
 *
 ************************************************************************/

void
cdetect_voutput(FILE *stream, const char *format, va_list arguments)
{
    cdetect_string_t message;

    message = cdetect_string_vformat(format, arguments);
    (void)fwrite(message->content, message->length, 1, stream);
    cdetect_string_destroy(message);
}

void
cdetect_fatal(const char *format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    cdetect_voutput(stderr, format, arguments);
    va_end(arguments);

    /* Program resources are not released */
    cdetect_exit(1);
}

void
cdetect_output(const char *format, ...)
{
    va_list arguments;

    if (cdetect_is_silent == CDETECT_FALSE) {
        va_start(arguments, format);
        cdetect_voutput(stdout, format, arguments);
        va_end(arguments);
    }
}

/* FIXME: Documentation */

int
config_report(const char *format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    cdetect_voutput(stdout, format, arguments);
    va_end(arguments);
    cdetect_output("\n");
    return 1;
}

void
cdetect_output_version(void)
{
    cdetect_output("cDetect %d.%d.%d\n",
                   CDETECT_VERSION_MAJOR,
                   CDETECT_VERSION_MINOR,
                   CDETECT_VERSION_PATCH);
}

void
cdetect_report_bool(const char *message, cdetect_report_t found)
{
    cdetect_output("checking for %s... %s%s\n",
                   message,
                   (found & CDETECT_REPORT_FOUND) ? "yes" : "no",
                   (found & CDETECT_REPORT_CACHED) ? " (cached)" : "");
}

/**
   Report boolean result

   @param message Message to be reported.
   @param found
*/

void
config_report_bool(const char *message, int found)
{
    cdetect_report_bool(message, found ? CDETECT_REPORT_FOUND : CDETECT_REPORT_NULL);
}

/* FIXME: Documentation */

void
config_report_string(const char *message, const char *value)
{
    cdetect_output("checking for %s... %s\n", message, value);
}

/*
 * Save to log file
 */

void
cdetect_log(const char *format, ...)
{
    va_list arguments;

    if (cdetect_is_verbose && !cdetect_is_silent) {
        va_start(arguments, format);
        cdetect_voutput(stdout, format, arguments);
        va_end(arguments);
    }
}

/*************************************************************************
 *
 * Substitution
 *
 ************************************************************************/

/*
 * Substitute a single variable
 */

cdetect_bool_t
cdetect_substitute_variable(cdetect_string_t source,
                            cdetect_string_t target,
                            size_t variable_before,
                            size_t variable_after,
                            size_t default_before,
                            size_t default_after,
                            cdetect_map_t variable_map)
{
    cdetect_bool_t success = CDETECT_FALSE;
    cdetect_string_t variable;
    cdetect_map_element_t element;

    assert(source != 0);
    assert(target != 0);

    if (variable_map) {
        variable = cdetect_string_create();
        if (variable) {
            (void)cdetect_string_append_range(variable, source->content, variable_before, variable_after);

            if (variable->content) {
                element = cdetect_map_lookup(variable_map, variable->content);
                if (element && element->data) {
                    /* Use stored value */
                    (void)cdetect_string_append(target, (const char *)element->data);
                    success = CDETECT_TRUE;
                } else if ((default_after > 0) && (default_before <= default_after)) {
                    /* Use default value */
                    (void)cdetect_string_append_range(target, source->content, default_before, default_after);
                    success = CDETECT_TRUE;
                }
            }
            cdetect_string_destroy(variable);
        }
    }
    return success;
}

/*
 * Substitute all variables in string
 */

cdetect_bool_t
cdetect_substitute_string(cdetect_string_t source,
                          cdetect_string_t *target)
{
    /* Copy source to target while replacing all known occurrences of @VARIABLE@ */
    size_t before = 0;
    size_t after = 0;
    size_t variable_before;
    size_t variable_after;
    size_t default_before;
    size_t default_after;
    size_t line_separator;
    cdetect_bool_t do_copy;

    assert(source != 0);
    assert(target != 0);

    *target = cdetect_string_format("");
    if (*target == 0) return CDETECT_FALSE;

    while (after < source->length) {
        /* Copy text before variable */
        after = cdetect_string_find_char(source, before, cdetect_variable_begin);
        (void)cdetect_string_append_range(*target, source->content, before, after);
        if (after == source->length)
            break;

        before = after + sizeof(cdetect_variable_begin);

        /* Substitute variable if exists, otherwise copy */
        after = cdetect_string_find_char(source, before, cdetect_variable_end);
        if (after == source->length) {
            do_copy = CDETECT_TRUE;
        }

        variable_before = before;
        variable_after = cdetect_string_find_char(source, before, cdetect_variable_default);
        if (variable_after < after) {
            default_after = after;
            default_before = variable_after + sizeof(cdetect_variable_default);
        } else {
            variable_after = after;
            default_before = 0;
            default_after = 0;
        }

        line_separator = cdetect_string_find_char(source, before, cdetect_variable_line);
        if (line_separator < after) {
            /* Newline found in variable. Skip over cdetect_variable_begin and try again. */
            do_copy = CDETECT_TRUE;
            after = before;
        } else {
            do_copy = !cdetect_substitute_variable(source,
                                                   *target,
                                                   variable_before,
                                                   variable_after,
                                                   default_before,
                                                   default_after,
                                                   cdetect_tool_map);
        }
        if (do_copy) {
            (void)cdetect_string_append_range(*target,
                                              source->content,
                                              before - sizeof(cdetect_variable_begin),
                                              after + sizeof(cdetect_variable_end));
        }

        before = after + sizeof(cdetect_variable_end);
        after = before;
    }

    return (after == source->length) ? CDETECT_TRUE : CDETECT_FALSE;
}

/*
 * Substitute all variables in file
 */

void
cdetect_substitute_file(const char *source,
                        const char *target)
{
    cdetect_string_t input;
    cdetect_string_t output;

    cdetect_log("cdetect_substitute_file(source = %'s, target = %'s)\n", source, target);

    if (cdetect_file_read(source, &input)) {

        if (cdetect_substitute_string(input, &output)) {
            (void)cdetect_file_remove(target);
            if (cdetect_file_overwrite(target, output) == CDETECT_FALSE) {
                cdetect_log("Cannot write file %s\n", target);
            }
            cdetect_output("creating %s (from %s)\n", target, source);
        }
        cdetect_string_destroy(output);
        cdetect_string_destroy(input);
    }
}

/*
 * Substitute all variables in all registered files
 */

void
cdetect_substitute_all_files(void)
{
    cdetect_map_element_t element;
    cdetect_list_t current;

    for (current = cdetect_list_front(cdetect_build_map->first);
         current != 0;
         current = cdetect_list_next(current)) {
        element = (cdetect_map_element_t)current->data;
        if (element) {
            cdetect_substitute_file(element->key, (const char *)element->data);
        }
    }
}

/*************************************************************************
 *
 * Execution and Compilation
 *
 ************************************************************************/

/*
 * Wrapper for system() to hide platform specific code
 */

cdetect_bool_t
cdetect_system(const char *command)
{
    cdetect_bool_t success = CDETECT_FALSE;
    int status;

    assert(command != 0);

#if defined(CDETECT_FUNC__FLUSHALL)

    /*
     * The system() documentation on MSDN states:
     *
     *  "You must explicitly flush (using fflush or _flushall) or close any
     *   stream before calling system."
     */
    (void)_flushall();

#endif

    status = system(command);

#if defined(WIFEXITED) && defined(WEXITSTATUS) && defined(WIFSIGNALED)

    /* Extract the proper exit code */
    if ((WIFEXITED(status)) && ((WEXITSTATUS(status)) == 0)) {
        success = CDETECT_TRUE;
    } else if (WIFSIGNALED(status)) {
        /* Terminate if signal was raised (e.g. SIGINT) */
        cdetect_fatal("Terminated\n");
    }

#else

    if (status == 0) {
        success = CDETECT_TRUE;
    }

#endif

    return success;
}

/*
 * Execute a command and return the status and the output
 */

cdetect_bool_t
cdetect_execute(cdetect_string_t command,
                cdetect_string_t *result,
                cdetect_bool_t is_remote)
{
    cdetect_bool_t success;
    cdetect_string_t full_command;
    cdetect_string_t redirection;

    cdetect_log("cdetect_execute(command = %'#^s, result, is_remote = %d)\n",
                command, (int)is_remote);

    if (is_remote && (cdetect_file_exist(cdetect_command_remote) == CDETECT_FALSE)) {
        success = CDETECT_FALSE;
        cdetect_log("cdetect_execute(command = %'^s) failed\n", command);
        cdetect_log("Remote execution not possible\n");

    } else {

        redirection = cdetect_file_unique_name(cdetect_file_redirection);

        if (is_remote) {
            full_command = cdetect_string_format(cdetect_format_remote,
                                                 cdetect_command_remote,
                                                 command->content,
                                                 redirection->content);
        } else {
            full_command = cdetect_string_format(cdetect_format_execute,
                                                 command->content,
                                                 redirection->content);
        }

        success = cdetect_system(full_command->content);

        (void)cdetect_file_read(redirection->content, result);

        (void)cdetect_file_remove(redirection->content);
        cdetect_string_destroy(redirection);
        cdetect_string_destroy(full_command);

        if (success == CDETECT_FALSE) {
            cdetect_log("cdetect_execute(%'^s) failed\n", command);
            cdetect_log(">>> OUTPUT BEGIN\n%s<<< OUTPUT END\n",
                        (*result) ? (*result)->content : "");
        }
    }
    return success;
}

cdetect_bool_t
cdetect_execute_substitute(cdetect_string_t command,
                           cdetect_string_t *result,
                           cdetect_bool_t is_remote)
{
    cdetect_bool_t success = CDETECT_FALSE;
    cdetect_string_t real_command = 0;

    if (cdetect_substitute_string(command, &real_command)) {

        success = cdetect_execute(real_command, result, is_remote);
    }

    cdetect_string_destroy(real_command);

    return success;
}

/*
 * Compile a file and optionally execute the resulting program
 */

cdetect_bool_t
cdetect_compile_file(cdetect_string_t source_file,
                     cdetect_string_t execute_file,
                     cdetect_string_t cflags,
                     cdetect_string_t ldflags,
                     cdetect_string_t arguments,
                     int do_execute,
                     int is_remote,
                     cdetect_string_t *result)
{
    cdetect_bool_t success;
    cdetect_string_t compile_command;
    cdetect_string_t execute_command = 0;

    assert(source_file != 0);
    assert(execute_file != 0);
    assert(result != 0);

    if (cdetect_command_compile == 0) {
        return CDETECT_FALSE;
    }

    (void)cdetect_file_remove(execute_file->content);

    compile_command = cdetect_string_format(cdetect_format_compile,
                                            cdetect_command_compile,
                                            cdetect_argument_cflags,
                                            cflags ? cflags->content : "",
                                            source_file->content,
                                            execute_file->content,
                                            ldflags ? ldflags->content : "");

    success = cdetect_execute(compile_command, result, CDETECT_FALSE);

    if (success && do_execute) {
        cdetect_string_destroy(*result);
        *result = 0;

        if (arguments) {
            execute_command = cdetect_string_format("%^s %^s", execute_file, arguments);
        } else {
            execute_command = cdetect_string_format("%^s", execute_file);
        }

        success = cdetect_execute(execute_command,
                                  result,
                                  (cdetect_bool_t)is_remote);
    }

    if (execute_command) {
        cdetect_string_destroy(execute_command);
    }
    cdetect_string_destroy(compile_command);

    return success;
}

/*
 * Write source code to file and compile
 */

cdetect_bool_t
cdetect_compile_source(cdetect_string_t sourcecode,
                       cdetect_string_t cflags,
                       cdetect_string_t ldflags,
                       cdetect_string_t arguments,
                       cdetect_bool_t do_execute,
                       cdetect_bool_t is_remote,
                       cdetect_string_t *result)
{
    cdetect_bool_t success;
    cdetect_string_t execute_file;
    cdetect_string_t source_file;

    cdetect_log(">>> SOURCE BEGIN\n%^s<<< SOURCE END\n", sourcecode);

    execute_file = cdetect_string_format("%s%s",
                                         cdetect_file_execute,
                                         cdetect_suffix_execute);
    source_file = cdetect_string_format("%s%s",
                                        cdetect_file_execute,
                                        cdetect_suffix_source);

    success = cdetect_file_overwrite(source_file->content, sourcecode);
    if (success == CDETECT_FALSE) {
        cdetect_log("Cannot write file %'^s\n",  source_file);
    } else {
        success = cdetect_compile_file(source_file,
                                       execute_file,
                                       cflags,
                                       ldflags,
                                       arguments,
                                       do_execute,
                                       is_remote,
                                       result);
        (void)cdetect_file_remove(execute_file->content);
    }

    (void)cdetect_file_remove(source_file->content);

    cdetect_string_destroy(source_file);
    cdetect_string_destroy(execute_file);

    return success;
}

/**
   Compile C/C++ source code.

   @param source Source code buffer.
   @param cflags Compilation flags.
   @return Boolean indicating success or failure to compile.
*/

int
config_compile_source(const char *source,
                      const char *cflags)
{
    cdetect_bool_t success;
    cdetect_string_t sourcecode;
    cdetect_string_t compile_flags;
    cdetect_string_t link_flags;
    cdetect_string_t result = 0;

    cdetect_log("config_compile_source(source, cflags = %'s)\n", cflags);

    /* FIXME: change cflags to comma-separated list of defines? */

    sourcecode = cdetect_string_format("%s", source);
    compile_flags = cdetect_string_format("%s", cflags);
    link_flags = cdetect_string_format("");

    success = cdetect_compile_source(sourcecode,
                                     compile_flags,
                                     link_flags,
                                     0,
                                     CDETECT_FALSE,
                                     CDETECT_FALSE,
                                     &result);

    cdetect_string_destroy(result);
    cdetect_string_destroy(link_flags);
    cdetect_string_destroy(compile_flags);
    cdetect_string_destroy(sourcecode);

    return (int)success;
}

int
config_file_create(const char *filename, const char *message)
{
    cdetect_bool_t success;
    cdetect_string_t content;

    content = cdetect_string_format("%s", message);

    success = cdetect_file_overwrite(filename, content);

    cdetect_string_destroy(content);

    return (int)success;
}

int
config_file_remove(const char *filename)
{
    return (int)cdetect_file_remove(filename);
}

int
config_file_exist_format(const char *format, ...)
{
    cdetect_bool_t success;
    cdetect_string_t name;
    va_list arguments;

    cdetect_log("config_file_exist(format = %'s)\n", format);

    va_start(arguments, format);
    name = cdetect_string_vformat(format, arguments);
    va_end(arguments);

    success = cdetect_file_exist(name->content);

    cdetect_string_destroy(name);

    return (int)success;
}

/**
   Compile a file containing C/C++ source code.

   @param filename Name of file to be compiled.
   @param cflags Compilation flags.
   @return Boolean indicating success or failure to compile.
*/

int
config_file_compile(const char *filename,
                    const char *cflags)
{
    cdetect_bool_t success;
    cdetect_string_t source_file;
    cdetect_string_t execute_file;
    cdetect_string_t compile_flags;
    cdetect_string_t link_flags;
    cdetect_string_t result = 0;

    cdetect_log("config_file_compile(filename = %'s, cflags = %'s)\n",
                filename, cflags);

    source_file = cdetect_string_format("%s", filename);
    execute_file = cdetect_string_format("%s%s",
                                         cdetect_file_execute,
                                         cdetect_suffix_execute);
    compile_flags = cdetect_string_format("%s", cflags);
    link_flags = cdetect_string_format("");

    success = cdetect_compile_file(source_file,
                                   execute_file,
                                   compile_flags,
                                   link_flags,
                                   0,
                                   CDETECT_FALSE,
                                   CDETECT_FALSE,
                                   &result);

    cdetect_string_destroy(result);
    cdetect_string_destroy(link_flags);
    cdetect_string_destroy(compile_flags);
    cdetect_string_destroy(execute_file);
    cdetect_string_destroy(source_file);

    return (int)success;
}

/**
   Compile and execute a file containing C/C++ source code.

   @param source Source code buffer.
   @param cflags Compilation flags.
   @param args Arguments for the execution.
   @return Boolean indicating success or failure to compile and execute.
*/

int
config_execute_source(const char *source,
                      const char *cflags,
                      const char *args)
{
    cdetect_bool_t success;
    cdetect_string_t sourcecode;
    cdetect_string_t compile_flags;
    cdetect_string_t link_flags;
    cdetect_string_t arguments;
    cdetect_string_t result = 0;

    cdetect_log("cdetect_execute_source(source, cflags = %'s, args = %'s)\n", cflags, args);

    /* FIXME: change cflags to comma-separated list of defines? */

    sourcecode = cdetect_string_format("%s", source);
    compile_flags = cdetect_string_format("%s", cflags);
    link_flags = cdetect_string_format("");
    arguments = cdetect_string_format("%s", args);

    success = cdetect_compile_source(sourcecode,
                                     compile_flags,
                                     link_flags,
                                     args ? arguments : 0,
                                     CDETECT_TRUE,
                                     (cdetect_bool_t)(cdetect_command_remote != 0),
                                     &result);

    cdetect_string_destroy(result);
    cdetect_string_destroy(arguments);
    cdetect_string_destroy(link_flags);
    cdetect_string_destroy(compile_flags);
    cdetect_string_destroy(sourcecode);

    return success;
}

/**
   Execute a command.

   @param command Name of command.
   @return Boolean indicating success or failure to execute.
*/
/* FIXME: document %-specifiers (@ref formatting) and @VAR@ substitution (@ref varsub) */

int
config_execute_command(const char *command, ...)
{
    cdetect_bool_t success = CDETECT_FALSE;
    cdetect_string_t real_command;
    cdetect_string_t result = 0;
    va_list arguments;

    cdetect_log("config_execute_command(command = %'#s, ...)\n", command);

    va_start(arguments, command);
    real_command = cdetect_string_vformat(command, arguments);
    va_end(arguments);

    success = cdetect_execute_substitute(real_command, &result, CDETECT_FALSE);

    cdetect_string_destroy(result);
    cdetect_string_destroy(real_command);

    return success;
}

/*************************************************************************
 *
 * Define Macros
 *
 ************************************************************************/

/*
 * Define a generic macro
 */

void
cdetect_macro_define(const char *macro,
                     const char *value)
{
    assert(macro != 0);
    assert(value != 0);

    (void)cdetect_map_remember_context(cdetect_macro_map, 0, macro, value);
}

/* FIXME: Documentation */

int
config_macro_define(const char *macro,
                    const char *value)
{
    cdetect_log("config_macro_define(macro = %'s, value = %'s)\n",
                macro, value);

    cdetect_macro_define(macro, value);

    return (int)CDETECT_TRUE;
}

/* FIXME: Documentation */

int
config_macro_define_format(const char *macro,
                           const char *format, ...)
{
    cdetect_string_t value;
    va_list arguments;

    cdetect_log("config_macro_define_format(macro = %'s, ...)\n",
                macro);

    va_start(arguments, format);
    value = cdetect_string_vformat(format, arguments);
    va_end(arguments);
    cdetect_macro_define(macro, value->content);

    cdetect_string_destroy(value);

    return (int)CDETECT_TRUE;
}

/*
 * Convert string into valid macro
 */

cdetect_string_t
cdetect_macro_transform_upper(cdetect_string_t macro)
{
    return cdetect_string_transform_upper(macro->content);
}

/*
 * Save macro
 */

void
cdetect_macro_save(const char *format,
                   const char *variable,
                   const char *value,
                   cdetect_macro_transform_t transform)
{
    cdetect_string_t data;
    cdetect_string_t raw_macro;
    cdetect_string_t macro;
    size_t offset;

    data = cdetect_string_format("%s", variable);
    offset = cdetect_string_find_char(data, 0, cdetect_map_context_separator);
    offset = (offset == data->length) ? 0 : offset + 1;

    raw_macro = cdetect_string_format(format, &data->content[offset]);
    if (transform) {
        macro = transform(raw_macro);
        cdetect_string_destroy(raw_macro);
    } else {
        macro = raw_macro;
    }

    cdetect_file_write_after(cdetect_include_file,
                             "#define %s %s\n",
                             macro->content, value);

    cdetect_string_destroy(macro);
    cdetect_string_destroy(data);
}

/*
 * Save macro with number
 */

void
cdetect_macro_save_number(const char *format,
                          const char *variable,
                          unsigned int number)
{
    cdetect_string_t value;

    if (variable) {
        value = cdetect_string_format("0x%x", number);
        cdetect_macro_save(format, variable, value->content, cdetect_macro_transform_upper);
        cdetect_string_destroy(value);
    }
}

/*
 * Save all macros in list
 */

void
cdetect_macro_save_map(cdetect_map_t map,
                       const char *format,
                       cdetect_macro_filter_t filter,
                       cdetect_macro_transform_t transform)
{
    cdetect_map_element_t element;
    cdetect_list_t current;

    for (current = cdetect_list_front(map->first);
         current != 0;
         current = cdetect_list_next(current)) {

        element = (cdetect_map_element_t)current->data;
        if (element) {
            if ((filter == 0) || filter(element->key, (const char *)element->data)) {
                cdetect_macro_save(format, element->key, (const char *)element->data, transform);
            }
        }
    }
}

/*
 * Filter macros with zero value
 */

cdetect_bool_t
cdetect_macro_filter_false(const char *key,
                           const char *value)
{
    assert(value != 0);
    (void)key;

    if (cdetect_strequal(value, "0"))
        return CDETECT_FALSE;
    return CDETECT_TRUE;
}

/*************************************************************************
 *
 * Detect Libraries
 *
 ************************************************************************/

/*
 * Define a library macro
 */

void
cdetect_library_define(const char *library,
                       cdetect_report_t found)
{
    (void)cdetect_map_remember(cdetect_library_map,
                               library,
                               (found & CDETECT_REPORT_FOUND) ? "1" : "0");
}

int
config_library_register_format(const char *format)
{
    cdetect_string_destroy(cdetect_library_format);
    cdetect_library_format = cdetect_string_format("%s", format);
    return (cdetect_library_format != 0);
}

/*************************************************************************
 *
 * Detect Functions
 *
 ************************************************************************/

/*
 * Define a function macro
 */

void
cdetect_function_define(const char *function,
                        const char *library,
                        cdetect_report_t found)
{
    assert(function != 0);

    (void)cdetect_map_remember_context(cdetect_function_map,
                                       library,
                                       function,
                                       (found & CDETECT_REPORT_FOUND) ? "1" : "0");
}

/* FIXME: Documentation */

void
config_function_define(const char *function,
                       const char *library,
                       int found)
{
    cdetect_function_define(function, library, found ? CDETECT_REPORT_FOUND : CDETECT_REPORT_NULL);
}

/*
 * Check if a function exists
 */

cdetect_report_t
cdetect_function_check_cache(const char *function,
                             const char *library)
{
    cdetect_report_t report = CDETECT_REPORT_NULL;
    cdetect_map_element_t element;

    element = cdetect_map_lookup_context(cdetect_function_map, library, function);
    if (element && element->data) {
        if (cdetect_strequal((const char *)element->data, "1")) {
            report = (cdetect_report_t)(CDETECT_REPORT_FOUND | CDETECT_REPORT_CACHED);
        } else {
            report = CDETECT_REPORT_CACHED;
        }
    }

    return report;
}

cdetect_report_t
cdetect_function_check_library(const char *function,
                               const char *library)
{
    cdetect_report_t report = CDETECT_REPORT_NULL;
    cdetect_string_t library_name;
    cdetect_string_t sourcecode;
    cdetect_string_t compile_flags;
    cdetect_string_t link_flags;
    cdetect_string_t result = 0;

    assert((library == 0) || (library[0] != 0)); /* Disallow the empty string */

    report = cdetect_function_check_cache(function, library);
    if (!(report & CDETECT_REPORT_CACHED)) {

        /* FIXME: Is both library_name and link_flags necessary? */
        if ((library == 0) || (library[0] == 0)) {
            library_name = cdetect_string_format("");
        } else {
            library_name = cdetect_string_format(cdetect_format_library, library);
        }

        sourcecode = cdetect_string_format("#ifdef __cplusplus\nextern \"C\"\n#endif\nchar %s();\nint main(void) {%s(); return 0;}\n",
                                           function, function);

        compile_flags = cdetect_string_format("");
        link_flags = cdetect_string_format("%^s", library_name);

        report = (cdetect_compile_source(sourcecode,
                                         compile_flags,
                                         link_flags,
                                         0,
                                         CDETECT_FALSE,
                                         CDETECT_FALSE,
                                         &result))
            ? CDETECT_REPORT_FOUND
            : CDETECT_REPORT_NULL;

        cdetect_string_destroy(result);
        cdetect_string_destroy(link_flags);
        cdetect_string_destroy(compile_flags);
        cdetect_string_destroy(sourcecode);
        cdetect_string_destroy(library_name);
    }
    return report;
}

/**
   Check for the existence of a given function in a given library.

   @param function The function to be examined.
   @param library Name of library in which the function resides.
   @return True (non-zero) if the function was found, false (zero) otherwise.
*/

int
config_function_check_library(const char *function,
                              const char *library)
{
    cdetect_report_t report = CDETECT_REPORT_NULL;
    cdetect_string_t message;

    cdetect_log("config_function_check_library(function = %'s, library = %'s)\n",
                function, library);

    if (function) {

        if ((library) && (library[0] == 0))
            library = 0;

        message = cdetect_string_format( (library == 0) ? "%s()" : "%s() in library %s",
                                         function,
                                         library);

        report = cdetect_function_check_library(function, library);
        cdetect_report_bool(message->content, report);
        cdetect_function_define(function, library, report);
        if (library) {
            /* If the function was found in a library, define this library as well */
            cdetect_library_define(library, report);
        }

        cdetect_string_destroy(message);
    }
    return (report & CDETECT_REPORT_FOUND);
}

/* FIXME: Documentation */

int
config_function_check(const char *function)
{
    return config_function_check_library(function, 0);
}

int
config_function_register_format(const char *format)
{
    cdetect_string_destroy(cdetect_function_format);
    cdetect_function_format = cdetect_string_format("%s", format);
    return (cdetect_function_format != 0);
}

/*************************************************************************
 *
 * Detect Headers
 *
 ************************************************************************/

/*
 * Define a header macro
 */

void
cdetect_header_define(const char *header,
                      cdetect_report_t found)
{
    (void)cdetect_map_remember(cdetect_header_map,
                               header,
                               (found & CDETECT_REPORT_FOUND) ? "1" : "0");
}

/* FIXME: Documentation */

void
config_header_define(const char *header,
                     int found)
{
    cdetect_header_define(header, found ? CDETECT_REPORT_FOUND : CDETECT_REPORT_NULL);
}

/*
 * Check if a header exists
 */

cdetect_report_t
cdetect_header_check(const char *header, const char *dependencies)
{
    cdetect_report_t report = CDETECT_REPORT_NULL;
    cdetect_map_element_t element;
    cdetect_string_t preclude;
    cdetect_string_t current;
    cdetect_string_t rest;
    cdetect_string_t work;
    cdetect_string_t sourcecode;
    cdetect_string_t compile_flags;
    cdetect_string_t link_flags;
    cdetect_string_t result = 0;

    element = cdetect_map_lookup(cdetect_header_map, header);
    if (element && element->data) {
        if (cdetect_strequal((const char *)element->data, "1")) {
            report = (cdetect_report_t)(CDETECT_REPORT_FOUND | CDETECT_REPORT_CACHED);
        } else {
            report = CDETECT_REPORT_CACHED;
        }

    } else {

        /* Build list of prerequisite headers */
        preclude = cdetect_string_create();
        if (dependencies) {

            current = cdetect_string_format("%s", dependencies);
            while (current) {
                rest = cdetect_string_split(current, cdetect_header_separator);
                if (cdetect_header_check(current->content, rest ? rest->content : 0)) {
                    cdetect_header_define(current->content, CDETECT_REPORT_FOUND);
                    work = cdetect_string_format("#include <%^s>\n", current);
                    (void)cdetect_string_append(preclude, work->content);
                    cdetect_string_destroy(work);
                }
                cdetect_string_destroy(current);
                current = rest;
            }
        }

        /* Examine if header file exists */
        sourcecode = cdetect_string_format("%^s#include <%s>\nint main(void) { return 0;}\n",
                                           preclude, header);

        compile_flags = cdetect_string_format("");
        link_flags = cdetect_string_format("");

        report = (cdetect_compile_source(sourcecode,
                                         compile_flags,
                                         link_flags,
                                         0,
                                         CDETECT_FALSE,
                                         CDETECT_FALSE,
                                         &result))
            ? CDETECT_REPORT_FOUND
            : CDETECT_REPORT_NULL;

        cdetect_string_destroy(result);
        cdetect_string_destroy(link_flags);
        cdetect_string_destroy(compile_flags);
        cdetect_string_destroy(sourcecode);
        cdetect_string_destroy(preclude);
    }

    return report;
}

/**
   Check for the existence of a given header file.

   @param header The header file to be examined.
   @return True (non-zero) if the header was found, false (zero) otherwise.
*/

int
config_header_check(const char *header)
{
    cdetect_report_t report = CDETECT_REPORT_NULL;
    cdetect_string_t message;

    cdetect_log("config_header_check(header = %'s)\n", header);

    if (header) {
        report = cdetect_header_check(header, 0);
        message = cdetect_string_format("<%s>", header);
        cdetect_report_bool(message->content, report);
        cdetect_header_define(header, report);

        cdetect_string_destroy(message);
    }
    return (report & CDETECT_REPORT_FOUND);
}

int
config_header_check_depend(const char *header,
                           const char *dependencies)
{
    cdetect_report_t report = CDETECT_REPORT_NULL;
    cdetect_string_t message;

    cdetect_log("config_header_check_depend(header = %'s, dependencies = %'s)\n",
                header, dependencies);

    if (header) {
        report = cdetect_header_check(header, dependencies);
        message = cdetect_string_format("<%s>", header);
        cdetect_report_bool(message->content, report);
        cdetect_header_define(header, report);

        cdetect_string_destroy(message);
    }
    return (report & CDETECT_REPORT_FOUND);
}

/* FIXME: Documentation */

int
config_header_register(const char *target)
{
    cdetect_string_destroy(cdetect_include_file);
    if (target == 0) {
        cdetect_include_file = 0;
    } else {
        cdetect_include_file = cdetect_string_format("%s", target);
        (void)cdetect_file_remove(cdetect_include_file->content);
    }
    return CDETECT_TRUE;
}

int
config_header_register_format(const char *format)
{
    cdetect_string_destroy(cdetect_header_format);
    cdetect_header_format = cdetect_string_format("%s", format);
    return (cdetect_header_format != 0);
}

/*
 * Save header file
 */

void
cdetect_header_save(void)
{
    cdetect_string_t raw_include_guard = 0;
    cdetect_string_t include_guard = 0;

    if (cdetect_include_file == 0)
        return;

    cdetect_log("cdetect_header_save(%'^s)\n", cdetect_include_file);

    cdetect_output("creating %^s\n", cdetect_include_file);

    cdetect_file_write_after(cdetect_include_file,
                             "/* Autogenerated by cDetect %d.%d.%d -- http://cdetect.sourceforge.net/ */\n",
                             CDETECT_VERSION_MAJOR,
                             CDETECT_VERSION_MINOR,
                             CDETECT_VERSION_PATCH);

    raw_include_guard = cdetect_string_format("CDETECT_INCLUDE_GUARD_%^s",
                                              cdetect_include_file);
    include_guard = cdetect_macro_transform_upper(raw_include_guard);

    if (include_guard) {
        cdetect_file_write_after(cdetect_include_file,
                                 "#ifndef %^s\n#define %^s\n\n",
                                 include_guard,
                                 include_guard);
    }

    if (cdetect_is_compiler_checked && cdetect_compiler_name) {
        cdetect_macro_save_number("CDETECT_COMPILER_%s",
                                  cdetect_compiler_name->content,
                                  cdetect_compiler_version);
    }
    if (cdetect_is_kernel_checked && cdetect_kernel_name) {
        cdetect_macro_save_number("CDETECT_KERNEL_%s",
                                  cdetect_kernel_name->content,
                                  cdetect_kernel_version);
    }
    if (cdetect_is_cpu_checked && cdetect_cpu_name) {
        cdetect_macro_save_number("CDETECT_CPU_%s",
                                  cdetect_cpu_name->content,
                                  cdetect_cpu_version);
    }

    cdetect_macro_save_map(cdetect_header_map,
                           cdetect_header_format->content,
                           cdetect_macro_filter_false,
                           cdetect_macro_transform_upper);
    cdetect_macro_save_map(cdetect_type_map,
                           cdetect_type_format->content,
                           cdetect_macro_filter_false,
                           cdetect_macro_transform_upper);
    cdetect_macro_save_map(cdetect_function_map,
                           cdetect_function_format->content,
                           cdetect_macro_filter_false,
                           cdetect_macro_transform_upper);
    cdetect_macro_save_map(cdetect_library_map,
                           cdetect_library_format->content,
                           cdetect_macro_filter_false,
                           cdetect_macro_transform_upper);
    cdetect_macro_save_map(cdetect_macro_map, "%s", 0, 0);

    if (include_guard) {
        cdetect_file_write_after(cdetect_include_file,
                                 "\n#endif /* %^s */\n",
                                 include_guard);
    }
    cdetect_string_destroy(include_guard);
    cdetect_string_destroy(raw_include_guard);
}

/*************************************************************************
 *
 * Detect Types
 *
 ************************************************************************/

/*
 * Define a type macro
 */

void
cdetect_type_define(const char *type,
                    const char *header,
                    cdetect_report_t found)
{
    assert(type != 0);

    (void)cdetect_map_remember_context(cdetect_type_map,
                                       header,
                                       type,
                                       (found & CDETECT_REPORT_FOUND) ? "1" : "0");
}

/**
   Define whether or not a data type exists.

   @param type The name of the data type.
   @param header The header file that contains the data type. Can be zero (0).
   @param found Boolean indicating whether or not the type was found.

   @sa config_type_check
*/

void
config_type_define(const char *type,
                   const char *header,
                   int found)
{
    cdetect_type_define(type, header, found ? CDETECT_REPORT_FOUND : CDETECT_REPORT_NULL);
}

/*
 * Check if a type exists
 */

cdetect_report_t
cdetect_type_check_cache(const char *type,
                         const char *header)
{
    cdetect_report_t report = CDETECT_REPORT_NULL;
    cdetect_map_element_t element;

    element = cdetect_map_lookup_context(cdetect_type_map, header, type);
    if (element && element->data) {
        if (cdetect_strequal((const char *)element->data, "1")) {
            report = (cdetect_report_t)(CDETECT_REPORT_FOUND | CDETECT_REPORT_CACHED);
        } else {
            report = CDETECT_REPORT_CACHED;
        }
    }

    return report;
}

cdetect_report_t
cdetect_type_check_header(const char *type,
                          const char *header)
{
    cdetect_report_t report = CDETECT_REPORT_NULL;
    cdetect_string_t sourcecode;
    cdetect_string_t compile_flags;
    cdetect_string_t link_flags;
    cdetect_string_t result = 0;

    report = cdetect_type_check_cache(type, header);
    if (!(report & CDETECT_REPORT_CACHED)) {

        if (header) {
            sourcecode = cdetect_string_format("#include <%s>\nint main(void) { int size;\nsize = sizeof(%s);\nreturn 0;}\n",
                                               header, type);
        } else {
            sourcecode = cdetect_string_format("int main(void) { int size;\nsize = sizeof(%s);\nreturn 0;}\n",
                                               type);
        }

        compile_flags = cdetect_string_format("");
        link_flags = cdetect_string_format("");

        report = (cdetect_compile_source(sourcecode,
                                         compile_flags,
                                         link_flags,
                                         0,
                                         CDETECT_FALSE,
                                         CDETECT_FALSE,
                                         &result))
            ? CDETECT_REPORT_FOUND
            : CDETECT_REPORT_NULL;

        cdetect_string_destroy(result);
        cdetect_string_destroy(link_flags);
        cdetect_string_destroy(compile_flags);
        cdetect_string_destroy(sourcecode);
    }

    return report;
}

/**
   Check for the existence of a given data type in a header file.

   @param type Data type to be examined.
   @param header Header file in which @p type may exist.
   @return True (non-zero) if the type was found, false (zero) otherwise.

   Macros for both the data type and the header are added to config.h if the
   data type exists.

   Example: Check if int64_t is located in <stddef.h> or <inttypes.h>

   @code
   config_type_check_header("int64_t", "stddef.h")
       || config_type_check_header("int64_t", "inttypes.h");
   @endcode

   which may result in

   @code
   #define CDETECT_HEADER_INTTYPES_H 1
   #define CDETECT_TYPE_INT64_T 1
   @endcode
*/

int
config_type_check_header(const char *type,
                         const char *header)
{
    cdetect_report_t report = CDETECT_REPORT_NULL;
    cdetect_string_t message;

    cdetect_log("config_type_check_header(type = %'s, header = %'s)\n",
                type, header);

    if (type) {

        if ((header) && (header[0] == 0))
            header = 0;

        report = cdetect_type_check_header(type, header);

        message = cdetect_string_format( (header == 0) ? "type %s" : "type %s in <%s>",
                                         type,
                                         header);
        cdetect_report_bool(message->content, report);
        cdetect_type_define(type, header, report);
        if (header) {
            /* If the type was found in a header, define this header as well */
            cdetect_header_define(header, report);
        }

        cdetect_string_destroy(message);
    }
    return (report & CDETECT_REPORT_FOUND);
}

/**
   Check for the existence of a given data type.

   @param type Data type to be examined.
   @return Boolean indicating if the data type was found.

   @sa config_type_check_header

   A macro for the data type is added to config.h if the data type exists.

   Example: Check for support of long long and __int64

   @code
   config_type_check("long long");
   config_type_check("__int64");
   @endcode

   which may result in

   @code
   #define CDETECT_TYPE_LONG_LONG 1
   #define CDETECT_TYPE___INT64 1
   @endcode
*/

int
config_type_check(const char *type)
{
    return config_type_check_header(type, 0);
}

int
config_type_register_format(const char *format)
{
    cdetect_string_destroy(cdetect_type_format);
    cdetect_type_format = cdetect_string_format("%s", format);
    return (cdetect_type_format != 0);
}

/*************************************************************************
 *
 * Detect Tools
 *
 ************************************************************************/

/*
 * Define a tool macro
 */

void
cdetect_tool_define(const char *variable,
                    const char *value)
{
    cdetect_string_t work;
    cdetect_string_t target = 0;

    work = cdetect_string_format("%s", value);
    if (!cdetect_substitute_string(work, &target)) {
        target = work;
        work = 0;
    }
    (void)cdetect_map_remember(cdetect_tool_map, variable, target->content);

    cdetect_string_destroy(work);
    cdetect_string_destroy(target);
}

/* FIXME: Documentation */

int
config_tool_define(const char *tool,
                   const char *value)
{
    cdetect_log("config_tool_define(tool = %'s, value = %'s)\n", tool, value);

    cdetect_tool_define(tool, value);
    return (int)CDETECT_TRUE;
}

int
config_tool_define_bool(const char *tool,
                        int value)
{
    cdetect_log("config_tool_define_bool(tool = %'s, value = %d)\n", tool, value);

    cdetect_tool_define(tool, (value ? "1" : "0"));
    return (int)CDETECT_TRUE;
}

int
config_tool_define_format(const char *tool,
                          const char *format, ...)
{
    cdetect_string_t value;
    va_list arguments;

    cdetect_log("config_tool_define_format(tool = %'s, format = %'s, ...)\n",
                tool, format);

    va_start(arguments, format);
    value = cdetect_string_vformat(format, arguments);
    va_end(arguments);
    cdetect_tool_define(tool, value->content);

    cdetect_string_destroy(value);

    return (int)CDETECT_TRUE;
}

/* FIXME: Documentation */

int
config_tool_define_command(const char *tool,
                           const char *command, ...)
{
    cdetect_bool_t success = CDETECT_FALSE;
    cdetect_string_t cmd;
    cdetect_string_t result = 0;
    va_list arguments;

    cdetect_log("config_tool_define_command(tool = %'s, command = %'s, ...)\n",
                tool, command);

    va_start(arguments, command);
    cmd = cdetect_string_vformat(command, arguments);
    va_end(arguments);

    if (cdetect_execute_substitute(cmd, &result, CDETECT_FALSE) && (result != 0)) {
        cdetect_string_trim(result, "\r\n");
        cdetect_tool_define(tool, result->content);
        success = CDETECT_TRUE;
    }
    cdetect_string_destroy(result);
    cdetect_string_destroy(cmd);

    return success;
}

/*
 * Check if a tool exists
 */

cdetect_bool_t
cdetect_tool_exist(const char *path)
{
    cdetect_bool_t success = CDETECT_FALSE;
    cdetect_string_t filename;
    cdetect_string_t remainder;

    if (path) {
        success = cdetect_file_exist(path);
        if (!success) {
            filename = cdetect_string_format("%s", path);
            /* Does not handle quoted spaces in path */
            remainder = cdetect_string_split(filename, ' ');
            success = cdetect_file_exist(filename->content);
            cdetect_string_destroy(remainder);
            cdetect_string_destroy(filename);
        }
    }
    return success;
}


const char *
cdetect_tool_check_with_path(const char *variable,
                             const char *path,
                             cdetect_tool_check_filter_t filter,
                             void *closure)
{
    cdetect_map_element_t element = 0;

    assert(path != 0);
    assert(variable != 0);

    element = cdetect_map_lookup(cdetect_tool_map, variable);
    if (element == 0) {
        if (cdetect_tool_exist(path) && (filter == 0 || filter(path, closure))) {
            cdetect_tool_define(variable, path);
        }
    }
    return (const char *)((element && element->data) ? element->data : 0);
}

const char *
cdetect_tool_check(const char *variable,
                   const char *tool,
                   cdetect_tool_check_filter_t filter,
                   void *closure)
{
    const char *result = 0;
    cdetect_string_t current;
    cdetect_string_t rest;
    cdetect_bool_t keep_trying = CDETECT_TRUE;

    if (tool) {

        current = cdetect_string_format("%s", cdetect_path);
        do {
            rest = cdetect_string_split(current, cdetect_path_list_separator);

            if (!cdetect_string_append_path(current, tool))
                keep_trying = CDETECT_FALSE;

            result = cdetect_tool_check_with_path(variable, current->content, filter, closure);
            if (result) {
                keep_trying = CDETECT_FALSE;
            } else if (rest == 0) {
                keep_trying = CDETECT_FALSE;
            }

            cdetect_string_destroy(current);
            current = rest;
        } while (keep_trying);

        cdetect_string_destroy(rest);
    }

    return result;
}

/* FIXME: Documentation */

const char *
config_tool_get(const char *tool)
{
    cdetect_map_element_t element;

    if ( (tool == 0) || (cdetect_tool_map == 0) ) {
        return 0;
    }

    element = cdetect_map_lookup(cdetect_tool_map, tool);
    return (element == 0)
        ? 0
        : (const char *)element->data;
}

/* FIXME: Documentation */

const char *
config_tool_check_filter(const char *variable,
                         const char *tool,
                         config_tool_check_filter_t filter,
                         void *closure)
{
    cdetect_string_t message;
    const char *result = 0;

    cdetect_log("config_tool_check_filter(variable = %'s, tool = %'s)\n",
                variable, tool);

    result = cdetect_tool_check(variable, tool, (cdetect_tool_check_filter_t)filter, closure);
    message = cdetect_string_format("tool %s", tool);
    config_report_string(message->content, result ? result : "no");
    cdetect_string_destroy(message);

    return result;
}

/* FIXME: Documentation */

const char *
config_tool_check(const char *variable,
                  const char *tool)
{
    return config_tool_check_filter(variable, tool, 0, 0);
}

/*************************************************************************
 *
 * Detect Compiler
 *
 ************************************************************************/

/*
 * Check if compilation works
 */

cdetect_bool_t
cdetect_check_compilation_filter(const char *command, void *userdata)
{
    cdetect_bool_t success;
    const char *arguments;
    char *backup;
    cdetect_string_t sourcecode;
    cdetect_string_t result;

    arguments = (const char *)userdata;

    /* Workaround is needed because cdetect_command_compile is global */
    backup = cdetect_command_compile;
    cdetect_command_compile = (char *)command;

    sourcecode = cdetect_string_format("int main(void) { return 0;}\n");

    success = cdetect_compile_source(sourcecode,
                                     0,
                                     0,
                                     0,
                                     CDETECT_TRUE,
                                     CDETECT_FALSE,
                                     &result);

    cdetect_command_compile = backup;
    cdetect_string_destroy(result);
    cdetect_string_destroy(sourcecode);

    return success;
}

const char *
cdetect_check_compilation_single(const char *envar,
                                 const char *command,
                                 const char *arguments)
{
    cdetect_string_t compiler;
    const char *result = 0;

    compiler = cdetect_string_format("%s%s", command, cdetect_suffix_execute);
    if (compiler) {
        result = cdetect_tool_check_with_path(envar,
                                              compiler->content,
                                              cdetect_check_compilation_filter,
                                              (void *)arguments);
        if (result == 0) {
            result = cdetect_tool_check(envar,
                                        compiler->content,
                                        cdetect_check_compilation_filter,
                                        (void *)arguments);
        }
        cdetect_string_destroy(compiler);
    }
    return result;
}

cdetect_bool_t
cdetect_check_compilation(const char *type,
                          const char *envar,
                          const char *compilers[][2])
{
    cdetect_bool_t success = CDETECT_FALSE;
    const char *command = 0;
    int current;

    cdetect_log("cdetect_check_compilation()\n");

    /* Check from option */
    if (cdetect_command_compile) {
        command = cdetect_check_compilation_single(envar, cdetect_command_compile, 0);
    }
    /* FIXME: environment */
    /* Check from built-in array */
    if (command == 0) {
        for (current = 0; compilers[current][0] != 0; ++current) {
            command = cdetect_check_compilation_single(envar,
                                                       compilers[current][0],
                                                       compilers[current][1]);
            if (command)
                break;
        }
    }

    /* FIXME: move outside this function */
    if (command == 0) {
        cdetect_output("checking for working %s compiler... none\n", type);
        if (cdetect_command_compile)
            cdetect_output("The %'s compiler does not work.\n", cdetect_command_compile);
        cdetect_output("Please run again with the --compiler option.\n");
    } else {
        cdetect_free(cdetect_command_compile);
        cdetect_command_compile = cdetect_strdup(command);
        success = CDETECT_TRUE;
        cdetect_output("checking for working %s compiler... %s\n", type, cdetect_command_compile);
    }

    return success;
}

/*
 * Check cross-compilation
 */

cdetect_bool_t
cdetect_check_cross_compilation(void)
{
    cdetect_bool_t success = CDETECT_FALSE;
    const char *source = "int main(void) { return 0; }\n";

    if (cdetect_command_remote) {

        cdetect_log("cdetect_check_cross_compilation(%'s)\n",
                    cdetect_command_remote);

        success = (cdetect_bool_t)config_execute_source(source, "", "");
        cdetect_report_bool("cross-compilation",
                            (success == CDETECT_TRUE) ? CDETECT_REPORT_FOUND : CDETECT_REPORT_NULL);
    }
    return success;
}

/*************************************************************************
 *
 * Detect Host (using chost.c)
 *
 ************************************************************************/

/*
 * Get host information
 */

cdetect_bool_t
cdetect_host(void)
{
    static cdetect_bool_t is_initialized = CDETECT_FALSE;
    cdetect_bool_t success;
    cdetect_string_t source_file;
    cdetect_string_t execute_file;
    cdetect_string_t compile_flags;
    cdetect_string_t link_flags;
    cdetect_string_t result = 0;
    cdetect_string_t compiler_name = 0;
    cdetect_string_t kernel_name = 0;
    cdetect_string_t cpu_name = 0;

    if (is_initialized)
        return CDETECT_TRUE;
    is_initialized = CDETECT_TRUE;

    source_file = cdetect_string_format(CDETECT_CHOST_FILE);
    execute_file = cdetect_string_format("%sb%s", /* Name purposely mangled */
                                         cdetect_file_execute,
                                         cdetect_suffix_execute);
    compile_flags = cdetect_string_format("");
    link_flags = cdetect_string_format("");

    cdetect_output("compiling %s...\n", source_file->content);

    success = cdetect_compile_file(source_file,
                                   execute_file,
                                   compile_flags,
                                   link_flags,
                                   0,
                                   CDETECT_TRUE,
                                   (cdetect_bool_t)(cdetect_command_remote != 0),
                                   &result);

    if (success && result) {

        if (cdetect_string_scan(result, "### %^[^:]:%x %^[^:]:%x %^[^:]:%x",
                                &compiler_name, &cdetect_compiler_version,
                                &kernel_name, &cdetect_kernel_version,
                                &cpu_name, &cdetect_cpu_version) == 6) {

            if (compiler_name->length > 0) {
                cdetect_compiler_name = compiler_name;
                compiler_name = 0;
            }

            if (kernel_name->length > 0) {
                cdetect_kernel_name = kernel_name;
                kernel_name = 0;
            }

            if (cpu_name->length > 0) {
                cdetect_cpu_name = cpu_name;
                cpu_name = 0;
            }
        }
        cdetect_string_destroy(cpu_name);
        cdetect_string_destroy(kernel_name);
        cdetect_string_destroy(compiler_name);
    }

    (void)cdetect_file_remove(execute_file->content);

    cdetect_string_destroy(result);
    cdetect_string_destroy(link_flags);
    cdetect_string_destroy(compile_flags);
    cdetect_string_destroy(execute_file);
    cdetect_string_destroy(source_file);

    return success;
}

/**
   Get name of compiler

   The name is determined by chost.c, which will automatically be compiled and
   executed the first time this function (or @c config_kernel or @c config_cpu)
   is called.The results are cached internally for subsequent use.

   The compilers that are currently supported are (see chost.c for a complete
   list)

   @li @c decc = Compaq C/C++
   @li @c gcc = GNU C/C++
   @li @c hpacc = HP aCC
   @li @c hpcc = HP ANSI C
   @li @c icc = Intel C/C++
   @li @c msc = Microsoft Visual Studio C/C++
   @li @c sunpro = Sun Studio C/C++

   @code
   if ( config_equal(config_compiler(), "sunpro") )
       printf("Using the Sun Studio C/C++ compiler\n");
   @endcode

   @return Lower-case string with the name of the compiler, or the null pointer
   (0) if the compiler could not be determined.

   @sa config_compiler_version
   @sa config_kernel
   @sa config_cpu
*/


const char *
config_compiler(void)
{
    if (!cdetect_host())
        return 0;

    return cdetect_compiler_name ? cdetect_compiler_name->content : 0;
}

/**
   Get version of compiler

   @code
   if ( config_equal(config_compiler(), "sunpro") &&
        ( config_compiler_version() < config_version(4, 2, 0) ) )
       printf("Older than Sun Studio 4.2\n");
   @endcode

   @return The version number of the compiler, or zero (0) if the compiler
   version could not be determined.

   @sa config_compiler
   @sa config_version
*/


unsigned int
config_compiler_version(void)
{
    if (!cdetect_host())
        return 0;

    return cdetect_compiler_version;
}

/* FIXME: Documentation */

int
config_compiler_check(void)
{
    cdetect_log("config_compiler_check()\n");

    if (!cdetect_host())
        return 0;

    if (cdetect_compiler_name == 0) {
        cdetect_output("checking compiler... Unknown\n");
    } else {
        if (cdetect_compiler_version == 0) {
            cdetect_output("checking compiler... %s\n", cdetect_compiler_name->content);
        } else {
            cdetect_output("checking compiler... %s %u.%u.%u\n",
                           cdetect_compiler_name->content,
                           (cdetect_compiler_version & 0xFF000000) >> 24,
                           (cdetect_compiler_version & 0x00FF0000) >> 16,
                           (cdetect_compiler_version & 0x0000FFFF));
        }
    }

    cdetect_is_compiler_checked = CDETECT_TRUE;

    return (cdetect_compiler_name != 0);
}

/*
 * Check C compiler
 */

cdetect_bool_t
cdetect_compiler_check_c(void)
{
    return (cdetect_check_compilation("C",
                                      "CC",
                                      cdetect_compilers_c));
}

/*
 * Check C++ compiler
 */

cdetect_bool_t
cdetect_compiler_check_cxx(void)
{
    return (cdetect_check_compilation("C++",
                                      "CXX",
                                      cdetect_compilers_cxx));
}

/*
 * Check preprocessor
 */

cdetect_bool_t
cdetect_compiler_check_cpp(void)
{
    return (cdetect_check_compilation("CPP",
                                      "CPP",
                                      cdetect_compilers_cpp));
}

/* FIXME: Documentation */

const char *
config_kernel(void)
{
    if (!cdetect_host())
        return 0;

    return cdetect_kernel_name ? cdetect_kernel_name->content : 0;
}

/* FIXME: Documentation */

unsigned int
config_kernel_version(void)
{
    if (!cdetect_host())
        return 0;

    return cdetect_kernel_version;
}

/* FIXME: Documentation */

int
config_kernel_check(void)
{
    cdetect_log("config_kernel_check()\n");

    if (!cdetect_host())
        return 0;

    if (cdetect_kernel_name == 0) {
        cdetect_output("checking kernel... Unknown\n");
    } else {
        if (cdetect_kernel_version == 0) {
            cdetect_output("checking kernel... %s\n", cdetect_kernel_name->content);
        } else {
            cdetect_output("checking kernel... %s %u.%u.%u\n",
                           cdetect_kernel_name->content,
                           (cdetect_kernel_version & 0xFF000000) >> 24,
                           (cdetect_kernel_version & 0x00FF0000) >> 16,
                           (cdetect_kernel_version & 0x0000FFFF));
        }
    }

    cdetect_is_kernel_checked = CDETECT_TRUE;

    return (cdetect_kernel_name != 0);
}

/* FIXME: Documentation */

const char *
config_cpu(void)
{
    if (!cdetect_host())
        return 0;

    return cdetect_cpu_name ? cdetect_cpu_name->content : 0;
}

/* FIXME: Documentation */

unsigned int
config_cpu_version(void)
{
    if (!cdetect_host())
        return 0;

    return cdetect_cpu_version;
}

/* FIXME: Documentation */

int
config_cpu_check(void)
{
    cdetect_log("config_cpu_check()\n");

    if (!cdetect_host())
        return 0;

    if (cdetect_cpu_name == 0) {
        cdetect_output("checking cpu... Unknown\n");
    } else {
        if (cdetect_cpu_version == 0) {
            cdetect_output("checking cpu... %s\n", cdetect_cpu_name->content);
        } else {
            cdetect_output("checking cpu... %s %u.%u.%u\n",
                           cdetect_cpu_name->content,
                           (cdetect_cpu_version & 0xFF000000) >> 24,
                           (cdetect_cpu_version & 0x00FF0000) >> 16,
                           (cdetect_cpu_version & 0x0000FFFF));
        }
    }

    cdetect_is_cpu_checked = CDETECT_TRUE;

    return (cdetect_cpu_name != 0);
}

/*************************************************************************
 *
 * Options
 *
 ************************************************************************/

/*
 * Create option
 */

cdetect_option_t
cdetect_option_create(void)
{
    cdetect_option_t self;

    self = (cdetect_option_t)cdetect_allocate(sizeof(*self));
    if (self) {
        self->short_name = 0;
        self->long_name = 0;
        self->default_value = 0;
        self->default_argument = 0;
        self->help = 0;
        self->callback = 0;
    }
    return self;

}

/*
 * Destroy option
 */

void
cdetect_option_destroy(cdetect_option_t self)
{
    if (self) {
        cdetect_free(self->long_name);
        cdetect_free(self->default_value);
        cdetect_free(self->default_argument);
        cdetect_free(self->help);
        cdetect_free(self);
    }
}

/*
 * Set option
 */

void
cdetect_option_set(cdetect_option_t self,
                   const char short_name,
                   const char *long_name,
                   const char *default_value,
                   const char *default_argument,
                   const char *help,
                   cdetect_option_callback_t callback)
{
    assert(self != 0);

    self->short_name = short_name;
    self->long_name = cdetect_strdup(long_name);
    self->default_value = cdetect_strdup(default_value);
    self->default_argument = cdetect_strdup(default_argument);
    self->help = cdetect_strdup(help);
    self->callback = callback;
}

/*
 * Get option from short name
 */

cdetect_option_t
cdetect_option_find_short(const char short_name)
{
    cdetect_list_t current;
    cdetect_map_element_t element;
    cdetect_option_t option;

    if ( (short_name == 0) || (cdetect_option_map == 0) ) {
        return 0;
    }

    for (current = cdetect_list_front(cdetect_option_map->first);
         current != 0;
         current = cdetect_list_next(current)) {
        element = (cdetect_map_element_t)current->data;
        if (element && element->data) {
            option = (cdetect_option_t)element->data;
            if (option->short_name == short_name) {
                return option;
            }
        }
    }
    return 0;
}

/*
 * Get option from long name
 */

cdetect_option_t
cdetect_option_get_long(const char *long_name)
{
    cdetect_map_element_t element;

    if ( (long_name == 0) || (cdetect_option_map == 0) )
        return 0;

    element = cdetect_map_lookup(cdetect_option_map, long_name);
    return (element) ? (cdetect_option_t)element->data : 0;
}

/* FIXME: Documentation */

const char *
config_option_get(const char *long_name)
{
    cdetect_map_element_t element;
    cdetect_option_t option;

    if ( (long_name == 0) || (cdetect_option_value_map == 0) )
        return 0;

    element = cdetect_map_lookup(cdetect_option_value_map, long_name);
    if (element) {

        /* Option specified by user */
        if (element->data)
            return (const char *)element->data;

        /* Option specified without argument. Use argument default */
        option = cdetect_option_get_long(long_name);
        if (option && option->default_argument)
            return option->default_argument;

    } else {

        /* Option not specified user. Use option default. */
        option = cdetect_option_get_long(long_name);
        if (option && option->default_value)
            return option->default_value;
    }
    return 0;
}

int
config_option_set(const char *long_name, const char *value)
{
    if ( (long_name == 0) || (value == 0) || (cdetect_option_value_map == 0) )
        return 0;

    return (cdetect_map_remember(cdetect_option_value_map, long_name, value) != 0);
}

/*
 * Register option group
 */

int
cdetect_option_register_group(const char *group)
{
    assert(cdetect_option_map != 0);

    if (group == 0)
        return CDETECT_FALSE;

    return (cdetect_map_remember(cdetect_option_map, group, 0) != 0);
}

/* FIXME: Documentation */

int
config_option_register_group(const char *group)
{
    return cdetect_option_register_group(group);
}

/*
 * Register command-line options
 */

int
cdetect_option_register(const char *long_name,
                        const char *short_name,
                        const char *default_value,
                        const char *default_argument,
                        const char *help,
                        cdetect_option_callback_t callback)
{
    cdetect_option_t option;

    assert(cdetect_option_map != 0);

    if ((short_name == 0) && (long_name == 0))
        return CDETECT_FALSE;

    option = cdetect_option_create();
    cdetect_option_set(option,
                       short_name ? short_name[0] : 0,
                       long_name,
                       default_value,
                       default_argument,
                       help,
                       callback);

    return (cdetect_map_remember(cdetect_option_map,
                                 long_name,
                                 option) != 0);
}

cdetect_bool_t
cdetect_option_user(const char *name, const char *argument)
{
    assert(cdetect_option_value_map != 0);

    return (cdetect_map_remember(cdetect_option_value_map,
                                 name,
                                 argument) != 0)
        ? CDETECT_TRUE
        : CDETECT_FALSE;
}

/**
   Register options.

   @param long_name Long option name.
   @param short_name Short option name.
   @param default_value Default value if option is not specified by user.
   @param default_argument Default value if option is specified without argument by user.
   @param help Help text.
   @return Boolean value indicating success or failure.

   The @p default_value and @p default_argument values are used to control if
   an argument should exist, and whether the argument is mandatory or optional.

   If @p default_value and @p default_argument are zero, then the option has
   no argument.

   If @p default_value is non-zero and @p default_argument is zero, then the
   argument is mandatory.

   If @p default_value is zero and @p default_argument is non-zero, then the
   argument is optional.
*/

int
config_option_register(const char *long_name,
                       const char *short_name,
                       const char *default_value,
                       const char *default_argument,
                       const char *help)
{
    cdetect_log("config_option_register(%'s, %'s, %'s, %'s, %'s)\n",
                long_name,
                short_name,
                default_value,
                default_argument,
                help);

    return cdetect_option_register(long_name,
                                   short_name,
                                   default_value,
                                   default_argument,
                                   help,
                                   cdetect_option_user);
}

/*
 * Show registered options
 */

void
cdetect_option_usage(void)
{
    cdetect_map_element_t element;
    cdetect_list_t current;
    cdetect_option_t option;
    int indent = 40;
    int skip;

    for (current = cdetect_list_front(cdetect_option_map->first);
         current != 0;
         current = cdetect_list_next(current)) {

        element = (cdetect_map_element_t)current->data;
        if (element) {

            option = (cdetect_option_t)element->data;
            if (option == 0) {
                /* Group */
                cdetect_output("%s:\n", element->key);

            } else {
                /* Option */
                skip = indent - 9 - strlen(element->key);
                if (indent < 0) indent = 0;
                cdetect_output("  %c%c%c --%s%-*s %s\n",
                               option->short_name ? '-' : ' ',
                               option->short_name ? option->short_name : ' ',
                               option->short_name ? ',' : ' ',
                               element->key,
                               skip,
                               option->default_value ? (option->default_argument ? " [<argument>]" : " <argument>") : "",
                               option->help ? option->help : "");
                if (option->default_value && option->default_value[0])
                    cdetect_output("%*sDefault value   : %s\n",
                                   indent,
                                   "",
                                   option->default_value);
                if (option->default_argument && option->default_argument[0])
                    cdetect_output("%*sDefault argument: %s\n",
                                   indent,
                                   "",
                                   option->default_argument);
            }
        }
    }
    cdetect_output("Default value is used if the option is omitted.\n");
    cdetect_output("Default argument is used if the option is used without an argument.\n");
}

/*************************************************************************
 *
 * Cache Functions
 *
 ************************************************************************/

/*
 * Save cache
 */

cdetect_string_t
cdetect_cache_encode(cdetect_map_element_t element,
                     const char *type)
{
    cdetect_string_t result;
    cdetect_string_t key;
    cdetect_string_t value;

    key = cdetect_string_escape(element->key,
                                cdetect_cache_separator,
                                cdetect_cache_escape);

    value = cdetect_string_escape((char *)element->data,
                                  cdetect_cache_separator,
                                  cdetect_cache_escape);

    result = cdetect_string_format("%s%c%^s%c%^s",
                                   type,
                                   cdetect_cache_separator,
                                   key,
                                   cdetect_cache_separator,
                                   value);

    cdetect_string_destroy(value);
    cdetect_string_destroy(key);

    return result;
}

void
cdetect_cache_encode_map(cdetect_map_t map,
                         const char *type)
{
    cdetect_list_t current;
    cdetect_map_element_t element;
    cdetect_string_t message;

    assert(map != 0);

    for (current = cdetect_list_front(map->first);
         current != 0;
         current = cdetect_list_next(current)) {

        element = (cdetect_map_element_t)current->data;
        if (element) {
            message = cdetect_cache_encode(element, type);
            cdetect_file_write_after(cdetect_cache_file->path, "%^s\n", message);
            cdetect_string_destroy(message);
        }
    }
}

void cdetect_cache_save(void)
{
    cdetect_string_t output;

    cdetect_log("cdetect_cache_save(%'^s)\n", cdetect_cache_file->path);

    output = cdetect_string_format("CDETECT%c%d.%d.%d\n",
                                   cdetect_cache_separator,
                                   CDETECT_VERSION_MAJOR,
                                   CDETECT_VERSION_MINOR,
                                   CDETECT_VERSION_PATCH);
    cdetect_file_overwrite(cdetect_cache_file->path->content, output);
    cdetect_cache_encode_map(cdetect_header_map, cdetect_cache_identifier_header);
    cdetect_cache_encode_map(cdetect_type_map, cdetect_cache_identifier_type);
    cdetect_cache_encode_map(cdetect_function_map, cdetect_cache_identifier_function);
    cdetect_cache_encode_map(cdetect_library_map, cdetect_cache_identifier_library);
    cdetect_string_destroy(output);
}

/*
 * Load cache
 */

void cdetect_cache_decode(cdetect_string_t line)
{
    cdetect_string_t format;
    cdetect_string_t type;
    cdetect_string_t escaped_key;
    cdetect_string_t escaped_value;
    cdetect_string_t key;
    cdetect_string_t value;

    if (line->length == 0)
        return;

    format = cdetect_string_format("%%^[^%c]%c%%^[^%c]%c%%^[^%c]",
                                   cdetect_cache_separator,
                                   cdetect_cache_separator,
                                   cdetect_cache_separator,
                                   cdetect_cache_separator,
                                   cdetect_cache_separator,
                                   cdetect_cache_separator);
    if (cdetect_string_scan(line, format->content, &type, &escaped_key, &escaped_value) == 3) {
        key = cdetect_string_unescape(escaped_key->content,
                                      cdetect_cache_escape);
        value = cdetect_string_unescape(escaped_value->content,
                                        cdetect_cache_escape);
        if (cdetect_strequal(type->content, cdetect_cache_identifier_header)) {
            cdetect_map_remember(cdetect_header_map, key->content, value->content);
        } else if (cdetect_strequal(type->content, cdetect_cache_identifier_function)) {
            cdetect_map_remember(cdetect_function_map, key->content, value->content);
        } else if (cdetect_strequal(type->content, cdetect_cache_identifier_library)) {
            cdetect_map_remember(cdetect_library_map, key->content, value->content);
        } else if (cdetect_strequal(type->content, cdetect_cache_identifier_type)) {
            cdetect_map_remember(cdetect_type_map, key->content, value->content);
        } else {
            cdetect_log("Unknown cache format: %'^s\n", line);
        }
        cdetect_string_destroy(value);
        cdetect_string_destroy(key);
        cdetect_string_destroy(escaped_value);
        cdetect_string_destroy(escaped_key);
        cdetect_string_destroy(type);
    }
    cdetect_string_destroy(format);
}

void cdetect_cache_load(void)
{
    cdetect_string_t input;
    cdetect_string_t rest;
    unsigned int offset;
    unsigned int major_version;
    unsigned int minor_version;
    unsigned int patch_version;

    cdetect_log("cdetect_cache_load(%'^s)\n", cdetect_cache_file->path);

    offset = 0;
    if (cdetect_file_read(cdetect_cache_file->path->content, &input)) {

        rest = cdetect_string_split(input, '\n');
        cdetect_string_scan(input, "CDETECT#%u.%u.%u",
                            &major_version, &minor_version, &patch_version);
        cdetect_string_destroy(input);
        input = rest;

        if (major_version == CDETECT_VERSION_MAJOR) {

            while (input) {
                rest = cdetect_string_split(input, '\n');
                cdetect_string_trim(input, "\r\n");
                cdetect_cache_decode(input);
                cdetect_string_destroy(input);
                input = rest;
            }

        } else {
            cdetect_string_destroy(rest);
        }
    }
}

/* FIXME: Documentation */

int
config_cache_register(const char *target)
{
    cdetect_file_destroy(cdetect_cache_file);
    cdetect_cache_file = cdetect_file_create();
    if (cdetect_cache_file == 0) {
        return CDETECT_FALSE;
    }
    cdetect_file_append(cdetect_cache_file, target);
    return CDETECT_TRUE;
}

/*************************************************************************
 *
 * Misc settings
 *
 ************************************************************************/

/* FIXME: Documentation */
/* FIXME: Explain variable substitution (and defaults) */
/* FIXME: '@' variable [ '=' default ] '@' */
/* FIXME: Explain that multiple calls adds files */

int
config_build_register(const char *source,
                      const char *target)
{
    assert(cdetect_build_map != 0);

    if (source && target) {
        if (cdetect_map_remember(cdetect_build_map, source, target))
            return CDETECT_TRUE;
    }
    return CDETECT_FALSE;
}

int
config_work_directory_register(const char *base)
{
    cdetect_string_destroy(cdetect_work_directory);
    cdetect_work_directory = cdetect_string_format("%s", base);
    /* FIXME: Modify work files */
    return (cdetect_work_directory != 0);
}

int
config_copyright_notice(const char *notice)
{
    cdetect_string_destroy(cdetect_copyright_notice);
    cdetect_copyright_notice = cdetect_string_format("%s", notice);
    return (cdetect_copyright_notice != 0);
}

/*************************************************************************
 *
 * Begin and End
 *
 ************************************************************************/

/*
 * Show program usage
 */

void cdetect_usage(void)
{
    cdetect_is_usage = CDETECT_TRUE;
    cdetect_option_usage();
}

void cdetect_fatal_wrong_option(const char *option)
{
    cdetect_fatal("Error in option %'s\n", option);
}

void cdetect_fatal_missing_option(const char *option)
{
    cdetect_fatal("Missing argument for option %'s\n", option);
}

/*
 * Parse command-line options
 */

cdetect_string_t
cdetect_parse_long_name(cdetect_array_t array,
                        cdetect_string_t input)
{
    cdetect_string_t name = 0;

    assert(array != 0);
    assert(input != 0);

    if (cdetect_string_scan(input, "--%^[^=]", &name) != 1) {
        cdetect_string_destroy(name);
        name = 0;
    }

    return name;
}

cdetect_string_t
cdetect_parse_long_argument(cdetect_array_t array,
                            cdetect_string_t input)
{
    /*
     * Parse long option according to one of the following formats:
     *
     *   "option argument"
     "   "option=argument"
     *   "option = argument"
     *
     * where option may be one long option.
     */
    cdetect_string_t argument = 0;
    const char *parameter;

    assert(array != 0);
    assert(input != 0);

    if (cdetect_string_contains_char(input, '=')) {

        /* Format is "option=argument" */
        if ((cdetect_string_scan(input, "--%*[^=]=%^s", &argument)) != 2) {
            cdetect_string_destroy(argument);
            argument = 0;
        }

    } else {

        parameter = cdetect_array_next(array);
        if (parameter) {
            if (parameter[0] == '-') {

                /* Parameter is a new option */
                (void)cdetect_array_former(array);
                parameter = 0;

            } else {

                if (cdetect_strequal("=", parameter)) {
                    /* Format is "option = argument" */
                    parameter = cdetect_array_next(array);
                } else {
                    /* Format is "option argument" */
                }
            }
        }
        if (parameter) {
            argument = cdetect_string_format("%s", parameter);
        }
    }

    return argument;
}

cdetect_bool_t
cdetect_parse_options(int argc, char *argv[])
{
    cdetect_array_t array;
    const char *current;
    int i;
    cdetect_string_t input;
    cdetect_string_t name;
    cdetect_string_t argument;
    cdetect_option_t option;
    const char *arg;

    array = cdetect_array_create((const char **)argv, argc);
    (void)cdetect_array_first(array); /* Skip argv[0] which is the program name */
    for (current = cdetect_array_next(array);
         current;
         current = cdetect_array_next(array)) {

        if (current[0] == '-') {

            if (current[1] == '-') {

                input = cdetect_string_format("%s", current);

                name = cdetect_parse_long_name(array, input);
                if (name == 0) {
                    cdetect_fatal_wrong_option(current);
                }
                option = cdetect_option_get_long(name->content);
                if (option == 0) {
                    cdetect_fatal_wrong_option(current);
                }

                argument = 0;
                if (option->default_value) {
                    argument = cdetect_parse_long_argument(array, input);
                    if (argument == 0) {
                        if (option->default_value[0] == 0) {
                            cdetect_fatal_missing_option(current);
                        }
                    }
                }

                if ((option->callback) &&
                    (!option->callback(name->content, argument ? argument->content : 0))) {
                    cdetect_exit(0);
                }
                cdetect_string_destroy(argument);
                cdetect_string_destroy(name);
                cdetect_string_destroy(input);


            } else {
                /*
                 * Parse short options according to the following format:
                 *
                 *   "options argument1 [argument2 ...]"
                 *
                 * where options may be one or more short options.
                 */

                for (i = 1; current[i] != 0; ++i) {

                    option = cdetect_option_find_short(current[i]);
                    if (option == 0) {
                        cdetect_fatal_wrong_option(current);
                    }

                    arg = 0;
                    if (option->default_value) {
                        arg = cdetect_array_next(array);
                        if (arg == 0) {
                            cdetect_fatal_missing_option(current);
                        }
                    }

                    if ((option->callback) &&
                        (!option->callback(option->long_name, arg))) {
                        cdetect_exit(0);
                    }
                }
            }
        }
    }
    cdetect_array_destroy(array);

    return CDETECT_TRUE;
}

/*
 * Initialize settings
 */

cdetect_bool_t
cdetect_initialize(void)
{
    char *environment;

    cdetect_output_version();

    if (system(0) == 0) {
        cdetect_fatal("Error: No command processor is available\n");
    }

    /* Read environment variables */
    environment = getenv("PATH");
    cdetect_path = cdetect_strdup(environment ? environment : "");

    if (cdetect_command_compile == 0) {
        environment = getenv("CC"); /* FIXME: Some other variable on non-Unix platforms? */
        if (environment) {
            cdetect_command_compile = cdetect_strdup(environment);
        }
        /* FIXME: CXX, CPP */
    }

    if (cdetect_argument_cflags == 0) {
        environment = getenv("CFLAGS");
        cdetect_argument_cflags = cdetect_strdup(environment ? environment : "");
        config_tool_define_format("CFLAGS", "@CFLAGS=@ %s", cdetect_argument_cflags);
    }

    /* Output compiler settings */
    cdetect_log("CC = %'s\n",
                (cdetect_command_compile) ? cdetect_command_compile : "");
    cdetect_log("CFLAGS = %'s\n", cdetect_argument_cflags);

    /* Sanity checks */
#if defined(__cplusplus)
    if (cdetect_compiler_check_cxx() == CDETECT_FALSE) {
        return CDETECT_FALSE;
    }
#else
    if (cdetect_compiler_check_c() == CDETECT_FALSE) {
        return CDETECT_FALSE;
    }
#endif
    if (cdetect_check_cross_compilation() == CDETECT_FALSE) {
        cdetect_command_remote = 0;
    }

    return CDETECT_TRUE;
}

/*
 * Perform command-line options
 */

cdetect_bool_t
cdetect_option_help(const char *name, const char *argument)
{
    (void)name;
    (void)argument;

    cdetect_usage();

    return CDETECT_FALSE;
}

cdetect_bool_t
cdetect_option_version(const char *name, const char *argument)
{
    (void)name;
    (void)argument;

    cdetect_output_version();
    if (cdetect_copyright_notice) {
        cdetect_output("%^s\n", cdetect_copyright_notice);
    }
    return CDETECT_FALSE;
}

cdetect_bool_t
cdetect_option_quiet(const char *name, const char *argument)
{
    (void)name;
    (void)argument;

    cdetect_is_silent = CDETECT_TRUE;

    return CDETECT_TRUE;
}

cdetect_bool_t
cdetect_option_verbose(const char *name, const char *argument)
{
    (void)name;
    (void)argument;

    cdetect_is_verbose = CDETECT_TRUE;

    return CDETECT_TRUE;
}

cdetect_bool_t
cdetect_option_dryrun(const char *name, const char *argument)
{
    (void)name;
    (void)argument;

    cdetect_is_dryrun = CDETECT_TRUE;

    return CDETECT_TRUE;
}

cdetect_bool_t
cdetect_option_compiler(const char *name, const char *argument)
{
    (void)name;

    cdetect_command_compile = cdetect_strdup(argument);

    return CDETECT_TRUE;
}

cdetect_bool_t
cdetect_option_cflags(const char *name, const char *argument)
{
    (void)name;

    cdetect_argument_cflags = cdetect_strdup(argument);
    config_tool_define_format("CFLAGS", "@CFLAGS=@ %s", argument);

    return CDETECT_TRUE;
}

cdetect_bool_t
cdetect_option_remote(const char *name, const char *argument)
{
    (void)name;

    cdetect_command_remote = cdetect_strdup(argument);

    return CDETECT_TRUE;
}

cdetect_bool_t
cdetect_option_refresh(const char *name, const char *argument)
{
    (void)name;
    (void)argument;

    cdetect_file_remove(cdetect_cache_file->path->content);

    return CDETECT_TRUE;
}

/*
 * Load results
 */

void
cdetect_load_files(void)
{
    cdetect_cache_load();
}

/*
 * Save results
 */

void
cdetect_save_files(void)
{
    cdetect_cache_save();
    cdetect_header_save();
    cdetect_substitute_all_files();
}

/*
 * Create all global variables
 */

void
cdetect_global_create(void)
{
    cdetect_option_map = cdetect_map_create(0, (cdetect_map_destroy_t)cdetect_option_destroy);
    cdetect_option_value_map = cdetect_map_create((cdetect_map_create_t)cdetect_strdup,
                                                  (cdetect_map_destroy_t)cdetect_free);

    cdetect_macro_map = cdetect_map_create((cdetect_map_create_t)cdetect_strdup,
                                           (cdetect_map_destroy_t)cdetect_free);
    cdetect_function_map = cdetect_map_create((cdetect_map_create_t)cdetect_strdup,
                                              (cdetect_map_destroy_t)cdetect_free);
    cdetect_header_map = cdetect_map_create((cdetect_map_create_t)cdetect_strdup,
                                            (cdetect_map_destroy_t)cdetect_free);
    cdetect_type_map = cdetect_map_create((cdetect_map_create_t)cdetect_strdup,
                                          (cdetect_map_destroy_t)cdetect_free);
    cdetect_library_map = cdetect_map_create((cdetect_map_create_t)cdetect_strdup,
                                             (cdetect_map_destroy_t)cdetect_free);

    cdetect_tool_map = cdetect_map_create((cdetect_map_create_t)cdetect_strdup,
                                          (cdetect_map_destroy_t)cdetect_free);
    cdetect_build_map = cdetect_map_create((cdetect_map_create_t)cdetect_strdup,
                                           (cdetect_map_destroy_t)cdetect_free);
}

/*
 * Destroy all global variables
 */

void
cdetect_global_destroy(void)
{
    cdetect_free(cdetect_path);
    cdetect_free(cdetect_command_remote);
    cdetect_free(cdetect_argument_cflags);
    cdetect_free(cdetect_command_compile);

    cdetect_string_destroy(cdetect_cpu_name);
    cdetect_string_destroy(cdetect_kernel_name);
    cdetect_string_destroy(cdetect_compiler_name);

    cdetect_map_destroy(cdetect_build_map);
    cdetect_string_destroy(cdetect_copyright_notice);
    cdetect_file_destroy(cdetect_cache_file);
    cdetect_string_destroy(cdetect_include_file);
    cdetect_string_destroy(cdetect_work_directory);
    cdetect_string_destroy(cdetect_header_format);
    cdetect_string_destroy(cdetect_function_format);
    cdetect_string_destroy(cdetect_library_format);
    cdetect_string_destroy(cdetect_type_format);

    cdetect_map_destroy(cdetect_tool_map);

    cdetect_map_destroy(cdetect_library_map);
    cdetect_map_destroy(cdetect_type_map);
    cdetect_map_destroy(cdetect_header_map);
    cdetect_map_destroy(cdetect_function_map);
    cdetect_map_destroy(cdetect_macro_map);

    cdetect_map_destroy(cdetect_option_value_map);
    cdetect_map_destroy(cdetect_option_map);
}

/**
   Begin detection.

   This function initializes cDetect, and must always be called before any
   other cDetect function is called.

   A cDetect program must be constructed according to the following template:
   @code
   #include "cdetect.c"

   int main(int argc, char *argv[])
   {
       config_begin();

       put_register_functions_here();

       if (config_options(argc, argv)) {

           put_detection_functions_here();
       }
       config_end();

       return 0;
   }
   @endcode

   @sa config_end
*/

void
config_begin(void)
{
    cdetect_global_create();

    if (!cdetect_is_nested) {
        cdetect_option_register_group("General");
        /* Long option, short option, default value, default argument, help text, callback */
        cdetect_option_register("help", "h", 0, 0, "Output this help text", cdetect_option_help);
        cdetect_option_register("version", "V", 0, 0, "Output cDetect version", cdetect_option_version);
        cdetect_option_register("quiet", "q", 0, 0, "Do not output to stdout", cdetect_option_quiet);
        cdetect_option_register("verbose", 0, 0, 0, "Output debugging information", cdetect_option_verbose);
        cdetect_option_register("no-create", "n", 0, 0, "No output files are created (dry run)", cdetect_option_dryrun);
        cdetect_option_register("refresh", 0, 0, 0, "Refresh cache", cdetect_option_refresh);
        cdetect_option_register("compiler", "c", "", 0, "Use argument as compiler", cdetect_option_compiler);
        cdetect_option_register("cflags", 0, "", 0, "Use argument as compile-time flags", cdetect_option_cflags);
        cdetect_option_register("remote", 0, "", 0, "Redirect execution to <argument>", cdetect_option_remote);
    }

    config_header_register("config.h");
    config_cache_register("cachect.txt");

    config_header_register_format("CDETECT_HEADER_%s");
    config_function_register_format("CDETECT_FUNC_%s");
    config_library_register_format("CDETECT_LIB_%s");
    config_type_register_format("CDETECT_TYPE_%s");
}

/**
   End detection.

   This function cleans up after cDetect, and must always be called after the
   detection is finished. No other cDetect function may be called afterwards.

   @sa config_begin
*/

void
config_end(void)
{
    /* Make sure CFLAGS variable exists */
    config_tool_get("CFLAGS") || config_tool_define("CFLAGS", "");

    if (!(cdetect_is_usage || cdetect_is_nested || cdetect_is_dryrun)) {
        cdetect_save_files();
    }
    cdetect_global_destroy();
}

/* FIXME: Documentation */

int
config_abort(void)
{
    cdetect_fatal("Aborting\n");
    return 1;
}

/**
   Handle command-line options.

   @param argc Number of arguments.
   @param argv Array of arguments.
   @return Boolean value indicating whether or not to proceed.

   This function interprets the command-line options.
   User-defined command-line options must have been registered before this
   function is called.

   @sa config_option_register
   @sa config_option_register_group
*/

int
config_options(int argc, char *argv[])
{
    if (cdetect_parse_options(argc, argv) && cdetect_initialize()) {

        cdetect_load_files();

        return (int)CDETECT_TRUE;
    }
    return (int)CDETECT_FALSE;
}

#endif /* CDETECT_C_INCLUDE_GUARD */
