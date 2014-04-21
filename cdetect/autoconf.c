/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*************************************************************************
 *
 * http://cdetect.sourceforge.net/
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

const char *ac_format_run_sh = "@SHELL=sh@ -c %'s";
const char *ac_format_run_python = "@PYTHON=python@ -c %'s";

/*
 * Register all options
 */

void ac_register(void)
{
    /* Macro template */

    config_header_register("config.h");
    config_header_register_format("HAVE_%s");
    config_function_register_format("HAVE_%s");
    config_library_register_format("HAVE_LIB%s");
    config_type_register_format("HAVE_%s");

    /* Built-in options */

    config_option_register_group("Installation");

    config_option_register("prefix", 0, "/usr/local", 0, "Installation prefix");
    config_option_register("exec_prefix", 0, "${prefix}", 0, "Executable installation prefix");

    config_option_register_group("Installation fine tuning");

    config_option_register("bindir", 0, "${exec_prefix}/bin", 0, "User executables");
    config_option_register("sbindir", 0, "${exec_prefix}/sbin", 0, "System administrator executables");
    config_option_register("libexecdir", 0, "${exec_prefix}/libexec", 0, "Program executables");
    config_option_register("datadir", 0, "${exec_prefix}/share", 0, "Read-only eneric data");
    config_option_register("sysconfdir", 0, "${prefix}/etc", 0, "Read-only machine data");
    config_option_register("sharedstatedir", 0, "${prefix}/com", 0, "Modifiable generic data");
    config_option_register("localstatedir", 0, "${prefix}/var", 0, "Modifiable machine data");
    config_option_register("libdir", 0, "${exec_prefix}/lib", 0, "Libraries");
    config_option_register("includedir", 0, "${prefix}/include", 0, "Header files");
    config_option_register("oldincludedir", 0, "/usr/include", 0, "Native header files");
    config_option_register("infodir", 0, "${prefix}/info", 0, "Info documentation");
    config_option_register("mandir", 0, "${prefix}/man", 0, "Man documentation");
}

void ac_prefix(void)
{
    if (config_option_get("prefix")) {
        config_tool_define("prefix", config_option_get("prefix"));
        config_tool_define("exec_prefix", config_option_get("exec_prefix"));
        config_tool_define("bindir", config_option_get("bindir"));
        config_tool_define("sbindir", config_option_get("sbindir"));
        config_tool_define("libexecdir", config_option_get("libexecdir"));
        config_tool_define("datadir", config_option_get("datadir"));
        config_tool_define("sysconfdir", config_option_get("sysconfdir"));
        config_tool_define("sharedstatedir", config_option_get("sharedstatedir"));
        config_tool_define("localstatedir", config_option_get("localstatedir"));
        config_tool_define("libdir", config_option_get("libdir"));
        config_tool_define("includedir", config_option_get("includedir"));
        config_tool_define("oldincludedir", config_option_get("oldincludedir"));
        config_tool_define("infodir", config_option_get("infodir"));
        config_tool_define("mandir", config_option_get("mandir"));
    }
}

void ac_init(const char *name,
             const char *version,
             const char *bugreport)
{
    config_macro_define("PACKAGE_NAME", name);
    config_macro_define("PACKAGE_VERSION", version);
    config_macro_define("PACKAGE_BUGREPORT", bugreport);
}

/*
 * Determine path of command
 */

static int ac_path_prog_filter(const char *tool, void *userdata)
{
    (void)tool;
    (void)userdata;

    return 1;
}

void ac_path_prog(const char *name,
                  const char *program,
                  const char *default_program)
{
    if (!config_tool_check_filter(name, program, ac_path_prog_filter, 0)) {
        config_tool_define(name, default_program);
    }
}

/*
 * Determine command-line shell
 */

void ac_prog_shell(void)
{
    config_tool_check("SHELL", "sh")
        || config_tool_check("SHELL", "ksh")
        || config_tool_check("SHELL", "bash");
}

/*
 * Determine egrep
 */

void ac_prog_egrep(void)
{
    if (config_execute_command(ac_format_run_sh, "echo a | grep -E '(a|b)'")) {
        config_tool_define("EGREP", "grep -E");
        config_report_string("egrep", "grep -E");

    } else if (config_execute_command(ac_format_run_sh, "echo a | egrep '(a|b)'")) {
        config_tool_define("EGREP", "egrep");
        config_report_string("egrep", "egrep");
    }
}

/*
 * Determine ranlib
 */

void ac_prog_ranlib(void)
{
    config_tool_check("RANLIB", "ranlib")
        || config_tool_define("RANLIB", ":");
}

/*
 * Determine awk processor
 */

void ac_prog_awk(void)
{
    config_tool_check("AWK", "gawk")
        || config_tool_check("AWK", "mawk")
        || config_tool_check("AWK", "nawk")
        || config_tool_check("AWK", "awk");
}

/*
 * Helper function for ac_prog_install
 */

static int ac_prog_install_filter(const char *tool, void *dummy)
{
    (void)dummy;

    /* Add broken or incompatible install programs here */

    if (config_equal(config_kernel(), "aix") && config_equal(tool, "/bin/install"))
        return 0;

    return 1;
}

/*
 * Determine install program
 */

void ac_prog_install(void)
{
    if (!config_tool_check_filter("INSTALL", "install", ac_prog_install_filter, 0)) {
        config_tool_define("INSTALL", "./install-sh");
    }
    config_tool_define("INSTALL", "@INSTALL@ -c");
    config_tool_define("INSTALL_PROGRAM", "${INSTALL}");
    config_tool_define("INSTALL_SCRIPT", "${INSTALL}");
    config_tool_define("INSTALL_DATA", "${INSTALL} -m 644");
}

/*
 * Helper function for ac_prog_ln_s
 */

static int ac_prog_ln_s_filter(const char *tool, void *userdata)
{
    int result = 0;
    const char *from_file = "cdetmpa.txt";
    const char *to_file = "cdetmpb.txt";

    (void)userdata;

    if (config_file_create(from_file, "")) {
        if (config_execute_command("%s %s %s", tool, from_file, to_file)) {
            if (config_file_exist_format("%s", to_file)) {
                result = 1;
            }
            config_file_remove(to_file);
        }
        config_file_remove(from_file);
    }
    return result;
}

/*
 * Determine symbolic link program
 */

void ac_prog_ln_s(void)
{
    config_tool_check_filter("LN_S", "ln -s", ac_prog_ln_s_filter, 0)
        || config_tool_check_filter("LN_S", "ln", ac_prog_ln_s_filter, 0)
        || config_tool_check_filter("LN_S", "cp -p", ac_prog_ln_s_filter, 0);
}

/*
 * Helper function for ac_path_python
 */

static int ac_path_python_filter(const char *tool, void *userdata)
{
    const char *expected_version;

    expected_version = (const char *)userdata;

    return ((expected_version == 0)
            || (*expected_version == 0)
            || (config_execute_command("%s -c \"import sys, string\nsys.exit(sys.hexversion < reduce(lambda x, y: (x << 8) + y, (map(int, string.split('%s', '.')) + [0,0,0,0])[:4]))\"", tool, expected_version)));
}

/*
 * Determine python, version, and paths
 */

void ac_path_python(const char *expected_version)
{
    static const char *names[] = { "python",    "python2",   "python2.5", "python2.4",
                                   "python2.3", "python2.2", "python2.1", "python2.0",
                                   "python1.6", "python1.5", 0 };
    int current;

    for (current = 0; names[current] != 0; ++current) {

        if (config_tool_check_filter("PYTHON", names[current], ac_path_python_filter, (void *)expected_version)) {

            config_tool_define_command("PYTHON_VERSION", ac_format_run_python, "import sys; print sys.version[:3]");

            config_tool_define_command("PYTHON_PREFIX", ac_format_run_python, "import sys; print sys.prefix")
                || config_tool_define("PYTHON_PREFIX", "@prefix=/usr/local@");

            config_tool_define_command("PYTHON_EXEC_PREFIX", ac_format_run_python, "import sys; print sys.exec_prefix")
                || config_tool_define("PYTHON_EXEC_PREFIX", "@exec_prefix=/usr/local@");

            config_tool_define_command("PYTHON_PLATFORM", ac_format_run_python, "import sys; print sys.platform");

            config_tool_define_command("pythondir", ac_format_run_python, "from distutils import sysconfig; print sysconfig.get_python_lib()") ||
                config_tool_define("pythondir", "@PYTHON_PREFIX@/lib/python@PYTHON_VERSION@/site-packages");

            config_tool_define("pkgpythondir", "${pythondir}/${PACKAGE}");

            config_tool_define_command("pyexecdir", ac_format_run_python, "from distutils import sysconfig; print sysconfig.get_python_lib()") ||
                config_tool_define("pyexecdir", "@PYTHON_EXEC_PREFIX@/lib/python@PYTHON_VERSION@/site-packages");

            config_tool_define("pkgpyexecdir", "${pyexecdir}/${PACKAGE}");

            break; /* for each name */
        }
    }
}

/*
 * Check ANSI C header files
 */

void ac_header_stdc(void)
{
    int success;

    success = config_compile_source("#include <stdlib.h>\n#include <stdarg.h>\n#include <string.h>\n#include <float.h>\nint main(void) { return 0; }\n", "");
    config_report_bool("ANSI C headers", success);
    if (success) {
        config_macro_define("STDC_HEADERS", "1");
    }
}

/*
 * Check time header files
 */

void ac_header_time(void)
{
    if (config_header_check_depend("time.h", "sys/time.h")) {
        config_macro_define("TIME_WITH_SYS_TIME", "1");
    } else {
        config_header_check("sys/time.h");
        config_header_check("time.h");
    }
}

/*
 * Check standard boolean type
 */

void ac_header_stdbool(void)
{
    config_type_check_header("_Bool", "stdbool.h")
        || config_type_check("_Bool");
}

/*
 * Check resolve header
 */

void ac_header_resolv(void)
{
    config_header_check_depend("resolv.h", "sys/types.h,netinet/in.h,arpa/nameser.h");
}

/*
 * Check dirent header
 */

void ac_header_dirent(void)
{
    config_header_check("dirent.h") ||
        config_header_check("sys/ndir.h") ||
        config_header_check("sys/dir.h") ||
        config_header_check("ndir.h");
}

void ac_header_stat(void)
{
    int success;

    if (config_header_check("sys/stat.h")) {
        success = config_compile_source("#include <sys/types.h>\n#include <sys/stat.h>\n#if defined(S_ISBLK) && defined(S_IFDIR)\n#if S_ISBLK(S_IFDIR)\nFailure\n#endif\n#endif\n#if defined(S_ISBLK) && defined(S_IFCHR)\n#if S_ISBLK(S_IFCHR)\nFailure\n#endif\n#endif\n#if defined(S_ISLNK) && defined(S_IFREG)\n#if S_ISLNK(S_IFREG)\nFailure\n#endif\n#endif\n#if defined(S_ISSOCK) && defined(S_IFREG)\n#if S_ISSOCK(S_IFREG)\nFailure\n#endif\n#endif\nint main(void) { return 0; }\n", "");
        config_report_bool("broken <sys/stat.h>", !success);
        if (!success) {
            config_macro_define("STAT_MACROS_BROKEN", "1");
        }
    }
}

void ac_header_mmap_anonymous(void)
{
    int success;

    success = config_compile_source("#include <sys/mman.h>\n#include <unistd.h>\n#include <fcntl.h>\nint main(void) { mmap(0, 1, PROT_READ, MAP_ANONYMOUS, -1, 0); return 0; }\n", "");
    config_report_bool("MMAP_ANONYMOUS", success);
    if (success) {
        config_header_define("mmap.h", 1);
        config_function_define("mmap", "", 1);
        config_macro_define("HAVE_MMAP_ANONYMOUS", "1");
    }
}

void ac_header_sys_wait(void)
{
    int success;

    success = config_compile_source("#include <sys/types.h>\n#include <sys/wait.h>\n#ifndef WEXITSTATUS\n#define WEXITSTATUS(x) ((unsigned int)(x) >> 8)\n#endif\n#ifndef WIFEXITED\n#define WIFEXITED(x) (((x) & 0xFF) == 0)\n#endif\nint main(void) { int s; wait(&s); return WIFEXITED(s) ? WEXITSTATUS(s) : 1; }\n", "");
    config_report_bool("<sys/wait.h> that is POSIX.1 compatible", success);
    if (success) {
        config_header_define("sys/wait.h", 1);
    }
}

/*
 * Check pid_t type
 */

void ac_type_pid_t(void)
{
    if ( !(config_type_check_header("pid_t", "stddef.h")
           || config_type_check_header("pid_t", "sys/types.h")
           || config_type_check_header("pid_t", "unistd.h")
           || config_type_check_header("pid_t", "sys/wait.h")
           || config_type_check_header("pid_t", "fcntl.h")
           || config_type_check_header("pid_t", "signal.h")
           || config_type_check("pid_t")) ) {
        config_macro_define("pid_t", "int");
    }
}

/*
 * Check uid_t type
 */

void ac_type_uid_t(void)
{
    config_type_check_header("uid_t", "sys/types.h")
        || config_type_check_header("uid_t", "unistd.h")
        || config_type_check("uid_t");
}

/*
 * Check off_t type
 */

void ac_type_off_t(void)
{
    if ( !(config_type_check_header("off_t", "sys/types.h")
           || config_type_check_header("off_t", "unistd.h")
           || config_type_check("off_t")) ) {
        config_macro_define("off_t", "long");
    }
}

/*
 * Check size_t type
 */

void ac_type_size_t(void)
{
    if ( !(config_type_check_header("size_t", "stddef.h")
           || config_type_check_header("size_t", "sys/types.h")
           || config_type_check_header("size_t", "unistd.h")
           || config_type_check("size_t")) ) {
        config_macro_define("size_t", "unsigned");
    }
}

/*
 * Check alloca function
 */

void ac_func_alloca(void)
{
    int success;
    char *source;

    /* FIXME: check for working alloca.h first */

    if (config_equal(config_compiler(), "gcc")) {
        source = "int main(void) {char *p = __builtin_alloca(1); return (p) ? 0 : 1; }\n";
    } else if (config_equal(config_compiler(), "msc")) {
        source = "#include <malloc.h>\nint main(void) { char *p = _alloca(1); return (p) ? 0 : 1; }\n";
    } else if (config_equal(config_compiler(), "xlc")) {
        source = "#pragma alloca\nint main(void) { char *p = alloca(1); return (p) ? 0 : 1; }\n";
    } else {
        source = "#ifndef alloca\nchar *alloca();\n#endif\nint main(void) { char *p = alloca(1); return (p) ? 0 : 1; }\n";
    }
    success = config_compile_source(source, "");
    config_report_bool("alloca", success);
    if (success) {
        config_function_define("alloca", "", 1);
    }
}

/*
 * Check *rand48 functions
 */

void ac_func_rand48(void)
{
    int success;

    success = config_compile_source("#include <stdlib.h>\nint main(void) { srand48(0); lrand48(); drand48(); return 0; }\n", "");
    config_report_bool("rand48 functions", success);
    if (success) {
        config_function_define("rand48", "", 1);
    }
}

/*
 * Check for the inline keyword
 */

void ac_c_inline(void)
{
    int success;

    success = config_compile_source("inline int foo(void) { return 1; }\nint main(void) { int i = foo(); return 0; }\n", "");
    if (!success) {
        success = config_compile_source("__inline__ int foo(void) { return 1; }\nint main(void) { int i = foo(); return 0; }\n", "");
        if (success) {
            config_macro_define("inline", "__inline__");
        } else {
            success = config_compile_source("__inline int foo(void) { return 1; }\nint main(void) { int i = foo(); return 0; }\n", "");
            if (success) {
                config_macro_define("inline", "__inline");
            } else {
                config_macro_define("inline", "");
            }
        }
    }
    config_report_bool("inline", success);
}

/*
 * Determine endianess
 */

int ac_c_bigendian()
{
    int success;

    success = config_execute_source("int main(void) { unsigned int i = 1; return (*((unsigned char *)&i) == 0) ? 0 : 1; }\n", "", "");
    config_report_bool("big endian", success);
    return success;
}

/*
 * Check if printf supports the %a formatting specifier
 */
void ac_c_printf_a(void)
{
    int success;

    success = config_compile_source("#include <stdlib.h>\n#include <stdio.h>\nint main(void) { volatile double alpha = 1.0/10.0; volatile double bravo; char buffer[100]; sprintf(buffer, \"%%a\", alpha); bravo = atof(buffer); if (alpha != bravo) return 1; if (alpha != 0x1.999999999999ap-4) return 1; return 0; }\n", "");
    config_report_bool("printf with %a format specifier", success);
    if (success) {
        config_function_define("printf_a", "", 1);
    }
}
