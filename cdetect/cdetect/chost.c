/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*************************************************************************
 *
 * http://cdetect.sourceforge.net/
 *
 * $Id: chost.c,v 1.10 2007/01/27 16:07:51 breese Exp $
 *
 * Copyright (C) 2005-2006 Bjorn Reese <breese@users.sourceforge.net>
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

/*************************************************************************
 *
 * For a list of pre-defined macros see http://predef.sourceforge.net/
 *
 ************************************************************************/

/*************************************************************************
 *
 * Tested
 *
 * 2006-12-25:
 *   GCC 3.4.2 - HP-UX B.11.11 - hppa
 *
 * 2006-08-28:
 *   CL 13.10.3007 - win32 5.2.3790 (Windows Server 2003) - i686
 *
 * 2006-08-20:
 *   GCC 3.2.2 - Linux 2.4.20 - i686
 *   GCC 2.96 - Linux 2.4.20 - i686
 *   GCC 3.4.2 - FreeBSD 5.4 - i386
 *   GCC 3.3.3 - NetBSD 2.0 - i386
 *   GCC 3.4.2 - Linux 2.6.9 - amd64
 *   GCC 2.95 - Linux 2.2.20 - alpha ev4
 *   GCC 3.3.3 - Linux 2.6.5 - powerpc
 *   GCC 3.1.0 - Darwin 6.8 - powerpc
 *   GCC 3.3.2 - SunOS 5.9 - sparc v8
 *   GCC 3.3.5 - OpenBSD 3.8 - i386
 *
 ************************************************************************/

#include <stdio.h>
#include "cdetect.c"

/*************************************************************************
 *
 * Detect Compiler
 *
 ************************************************************************/

#if defined(__DECCXX)
# if defined(__DECCXX_VER)
#  define PREDEF_COMPILER_DECC CDETECT_MKVER(__DECCXX_VER / 10000000, (__DECCXX_VER % 10000000) / 100000, __DECCXX_VER % 1000)
# else
#  define PREDEF_COMPILER_DECC CDETECT_MKVER(0, 0, 0)
# endif
#else
# if defined(__DECC)
#  if defined(__DECC_VER)
#   define PREDEF_COMPILER_DECC CDETECT_MKVER(__DECC_VER / 10000000, (__DECC_VER % 10000000) / 100000, __DECC_VER % 1000)
#  else
#   define PREDEF_COMPILER_DECC CDETECT_MKVER(0, 0, 0)
#  endif
# endif
#endif

#if defined(__GNUC__)
# if defined(__GNUC_PATCHLEVEL__)
#  define PREDEF_COMPILER_GCC CDETECT_MKVER(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
# else
#  define PREDEF_COMPILER_GCC CDETECT_MKVER(__GNUC__, __GNUC_MINOR__, 0)
# endif
#endif

#if defined(__HP_aCC)
# if (__HP_aCC == 1)
#  define PREDEF_COMPILER_HPACC CDETECT_MKVER(1, 15, 0)
# else
#  define PREDEF_COMPILER_HPACC CDETECT_MKVER(__HP_aCC / 10000, (__HP_aCC % 10000) / 100, __HP_aCC % 100)
# endif
#endif

#if defined(__HP_cc)
# define PREDEF_COMPILER_HPCC CDETECT_MKVER(__HP_cc / 10000, (__HP_cc % 10000) / 100, __HP_cc % 100)
#endif

#if defined(__INTEL_COMPILER)
# define PREDEF_COMPILER_ICC CDETECT_MKVER(__INTEL_COMPILER / 100, (__INTEL_COMPILER % 100) / 10, __INTEL_COMPILER % 10)
#else
# if defined(__ICC)
#  define PREDEF_COMPILER_ICC CDETECT_MKVER(__ICC / 100, (__ICC % 100) / 10, __ICC % 10)
# else
#  if defined(__ICL)
#   define PREDEF_COMPILER_ICC CDETECT_MKVER(0, 0, 0)
#  endif
# endif
#endif

#if defined(_MSC_FULL_VER)
# define PREDEF_COMPILER_MSC CDETECT_MKVER(_MSC_FULL_VER / 1000000, (_MSC_FULL_VER % 1000000) / 10000, _MSC_FULL_VER % 10000)
#else
# if defined(_MSC_VER)
#  define PREDEF_COMPILER_MSC CDETECT_MKVER(_MSC_VER / 100, _MSC_VER % 100, 0)
# endif
#endif

#if defined(__SUNPRO_CC)
# define PREDEF_COMPILER_SUNPRO CDETECT_MKVER(__SUNPRO_CC / 0x100, (__SUNPRO_CC % 0x100) / 0x10, __SUNPRO_CC % 0x10)
#else
# if defined(__SUNPRO_C)
# define PREDEF_COMPILER_SUNPRO CDETECT_MKVER(__SUNPRO_C / 0x100, (__SUNPRO_C % 0x100) / 0x10, __SUNPRO_C % 0x10)
# endif
#endif

/*----------------------------------------------------------------------*/

cdetect_bool_t
chost_compiler_get(cdetect_string_t *name,
                   unsigned int *version)
{
#if defined(PREDEF_COMPILER_DECC)
    *name = cdetect_string_format("decc");
    *version = PREDEF_COMPILER_DECC;

#else
#if defined(PREDEF_COMPILER_GCC)
    *name = cdetect_string_format("gcc");
    *version = PREDEF_COMPILER_GCC;

#else
#if defined(PREDEF_COMPILER_HPACC)
    *name = cdetect_string_format("hpacc");
    *version = PREDEF_COMPILER_HPACC;

#else
#if defined(PREDEF_COMPILER_HPCC)
    *name = cdetect_string_format("hpcc");
    *version = PREDEF_COMPILER_HPCC;

#else
#if defined(PREDEF_COMPILER_ICC)
    *name = cdetect_string_format("icc");
    *version = PREDEF_COMPILER_ICC;

#else
#if defined(PREDEF_COMPILER_MSC)
    *name = cdetect_string_format("msc");
    *version = PREDEF_COMPILER_MSC;

#else
#if defined(PREDEF_COMPILER_SUNPRO)
    *name = cdetect_string_format("sunpro");
    *version = PREDEF_COMPILER_SUNPRO;

#else
    *name = 0;
    *version = 0;

#endif /* PREDEF_COMPILER_SUNPRO */
#endif /* PREDEF_COMPILER_MSC */
#endif /* PREDEF_COMPILER_ICC */
#endif /* PREDEF_COMPILER_HPCC */
#endif /* PREDEF_COMPILER_HPACC */
#endif /* PREDEF_COMPILER_GCC */
#endif /* PREDEF_COMPILER_DECC */

    return (*name != 0) ? CDETECT_TRUE : CDETECT_FALSE;
}

/*************************************************************************
 *
 * Detect Kernel
 *
 ************************************************************************/

#if defined(__FreeBSD__)
# define PREDEF_OS_FREEBSD
#endif

#if defined(hpux) || defined(__hpux)
# define PREDEF_OS_HPUX
#endif

#if defined(linux) || defined(__linux) || defined(__linux__)
# define PREDEF_OS_LINUX
#endif

#if defined(__APPLE__) && defined(__MACH__)
# define PREDEF_OS_DARWIN
#endif

#if defined(__NetBSD__)
# define PREDEF_OS_NETBSD
#endif

#if defined(__OpenBSD__)
# define PREDEF_OS_OPENBSD
#endif

#if defined(__osf__) || defined(__osf)
# define PREDEF_OS_OSF
#endif

#if defined(sun) || defined(__sun__)
# if defined(__SVR4) || defined(__svr4__)
#  define PREDEF_OS_SOLARIS
# else
#  define PREDEF_OS_SUNOS
# endif
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
# define PREDEF_OS_WIN32
#endif

/*----------------------------------------------------------------------*/

#if !defined(PREDEF_OS_WIN32)

/*
 * Read and scan kernel version from command
 */

unsigned int
chost_kernel_version(const char *version_command,
                     const char *version_format)
{
    unsigned int result = 0;
    cdetect_string_t execute;
    cdetect_string_t execute_result;
    unsigned int major_version = 0;
    unsigned int minor_version = 0;
    unsigned int patch_version = 0;
  
    execute = cdetect_string_format("%s", version_command); 
    if (cdetect_execute(execute, &execute_result, CDETECT_FALSE)) {
        cdetect_string_scan(execute_result,
                            version_format,
                            &major_version,
                            &minor_version,
                            &patch_version);
        result = CDETECT_MKVER(major_version,
                               minor_version,
                               patch_version);
        cdetect_string_destroy(execute_result);
    }
    cdetect_string_destroy(execute);

    return result;
}

#endif

/*----------------------------------------------------------------------*/

cdetect_bool_t
chost_kernel_get(cdetect_string_t *name,
                 unsigned int *version)
{

#if defined(PREDEF_OS_DARWIN)
    *name = cdetect_string_format("darwin");
    *version = chost_kernel_version("uname -r", "%u.%u");

#else
#if defined(PREDEF_OS_FREEBSD)
    *name = cdetect_string_format("freebsd");
    *version = chost_kernel_version("uname -r", "%u.%u.%u");

#else
#if defined(PREDEF_OS_HPUX)
    *name = cdetect_string_format("hpux");
    *version = chost_kernel_version("uname -r", "%*[^.].%u.%u");

#else
#if defined(PREDEF_OS_LINUX)
    *name = cdetect_string_format("linux");
    *version = chost_kernel_version("uname -r", "%u.%u.%u");

#else
#if defined(PREDEF_OS_NETBSD)
    *name = cdetect_string_format("netbsd");
    *version = chost_kernel_version("uname -r", "%u.%u");

#else
#if defined(PREDEF_OS_OPENBSD)
    *name = cdetect_string_format("openbsd");
    *version = chost_kernel_version("uname -r", "%u.%u.%u");

#else
#if defined(PREDEF_OS_OSF)
    *name = cdetect_string_format("osf");
    *version = chost_kernel_version("uname -rv", "V%u.%u.%u");

#else
#if defined(PREDEF_OS_SUNOS) || defined(PREDEF_OS_SOLARIS)
    *name = cdetect_string_format("sunos");
    *version = chost_kernel_version("uname -r", "%u.%u");

#else
#if defined(PREDEF_OS_WIN32)
    DWORD dwVersion;

    *name = cdetect_string_format("win32");
  
    dwVersion = GetVersion();
    *version = CDETECT_MKVER( LOBYTE(LOWORD(dwVersion)),
                              HIBYTE(LOWORD(dwVersion)),
                              ((dwVersion < 0x80000000) ? HIWORD(dwVersion) : 0) );

#else

    *name = 0;
    *version = 0;

#endif /* PREDEF_OS_WIN32 */
#endif /* PREDEF_OS_SUNOS / SOLARIS */
#endif /* PREDEF_OS_OSF */
#endif /* PREDEF_OS_OPENBSD */
#endif /* PREDEF_OS_NETBSD */
#endif /* PREDEF_OS_LINUX */
#endif /* PREDEF_OS_HPUX */
#endif /* PREDEF_OS_FREEBSD */
#endif /* PREDEF_OS_DARWIN */

    return (*name != 0) ? CDETECT_TRUE : CDETECT_FALSE;
}

/*************************************************************************
 *
 * Detect CPU
 *
 ************************************************************************/

#if defined(__alpha_ev6__)
# define PREDEF_CPU_ALPHA CDETECT_MKVER(6, 0, 0)
#else
# if defined(__alpha_ev5__)
#  define PREDEF_CPU_ALPHA CDETECT_MKVER(5, 0, 0)
# else
#  if defined(__alpha_ev4__)
#   define PREDEF_CPU_ALPHA CDETECT_MKVER(4, 0, 0)
#  else
#   if defined(__alpha__) || defined(__alpha) || defined(_M_ALPHA)
#    define PREDEF_CPU_ALPHA CDETECT_MKVER(0, 0, 0)
#   endif
#  endif
# endif
#endif

#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64)
# define PREDEF_CPU_AMD64 CDETECT_MKVER(0, 0, 0)
#endif

#if defined(__arm__) && !defined(__ARMCC_VERSION)
# define PREDEF_CPU_ARM CDETECT_MKVER(0, 0, 0)
#else
# if defined(__TARGET_ARCH_ARM)
#  define PREDEF_CPU_ARM CDETECT_MKVER(__TARGET_ARCH_ARM, 0, 0)
# endif
#endif

#if defined(__hppa__) || defined(__hppa)
# if defined(_PA_RISC1_0)
#  define PREDEF_CPU_HPPA CDETECT_MKVER(1, 0, 0)
# else
#  if defined(_PA_RISC1_1)
#   define PREDEF_CPU_HPPA CDETECT_MKVER(1, 1, 0)
#  else
#   if defined(_PA_RISC2_0) || defined(__RISC2_0__)
#    define PREDEF_CPU_HPPA CDETECT_MKVER(2, 0, 0)
#   else
#    define PREDEF_CPU_HPPA CDETECT_MKVER(0, 0, 0)
#   endif
#  endif
# endif
#endif

#if defined(__m68k__)
# if defined(__mc68060__)
#  define PREDEF_CPU_M68K CDETECT_MKVER(6, 0, 0)
# else
#  if defined(__mc68040__)
#   define PREDEF_CPU_M68K CDETECT_MKVER(4, 0, 0)
#  else
#   if defined(__mc68030__)
#    define PREDEF_CPU_M68K CDETECT_MKVER(3, 0, 0)
#   else
#    if defined(__mc68020__)
#     define PREDEF_CPU_M68K CDETECT_MKVER(2, 0, 0)
#    else
#     if defined(__mc68010__)
#      define PREDEF_CPU_M68K CDETECT_MKVER(1, 0, 0)
#     else
#      define PREDEF_CPU_M68K CDETECT_MKVER(0, 0, 0)
#     endif
#    endif
#   endif
#  endif
# endif
#else
# if defined(M68000)
#  define PREDEF_CPU_M68K CDETECT_MKVER(0, 0, 0)
# endif
#endif

#if defined(__mips__)
# if defined(_MIPS_ISA)
#  if (_MIPS_ISA == _MIPS_ISA_MIPS4)
#   define PREDEF_CPU_MIPS CDETECT_MKVER(4, 0, 0)
#  else
#   if (_MIPS_ISA == _MIPS_ISA_MIPS3)
#    define PREDEF_CPU_MIPS CDETECT_MKVER(3, 0, 0)
#   else
#    if (_MIPS_ISA == _MIPS_ISA_MIPS2)
#     define PREDEF_CPU_MIPS CDETECT_MKVER(2, 0, 0)
#    else
#     if (_MIPS_ISA == _MIPS_ISA_MIPS1)
#      define PREDEF_CPU_MIPS CDETECT_MKVER(1, 0, 0)
#     else
#      define PREDEF_CPU_MIPS CDETECT_MKVER(0, 0, 0)
#     endif
#    endif
#   endif
#  endif
# else
#  define PREDEF_CPU_MIPS CDETECT_MKVER(0, 0, 0)
# endif
#else
# if defined(__mips)
#  define PREDEF_CPU_MIPS CDETECT_MKVER(__mips, 0, 0)
# else
#  if defined(__MIPS__)
#   define PREDEF_CPU_MIPS CDETECT_MKVER(0, 0, 0)
#  endif
# endif
#endif

#if defined(_M_PPC)
# define PREDEF_CPU_POWERPC CDETECT_MKVER(_M_PPC / 100, _M_PPC % 100, 0)
#else
# if defined(__ppc604__) || defined(__ARCH_604)
#  define PREDEF_CPU_POWERPC CDETECT_MKVER(6, 4, 0)
# else
#  if defined(__ppc603__) || defined(__ARCH_603)
#   define PREDEF_CPU_POWERPC CDETECT_MKVER(6, 3, 0)
#  else
#   if defined(__ppc601__) || defined(__ppc601) || defined(__ARCH_601)
#    define PREDEF_CPU_POWERPC CDETECT_MKVER(6, 1, 0)
#   else
#    if defined(__powerpc) || defined(__powerpc__) || defined(__POWERPC__) || defined(__ppc__) || defined(__PPC) || defined(__PPC__) || defined(__ARCH_PPC)
#     define PREDEF_CPU_POWERPC CDETECT_MKVER(0, 0, 0)
#    endif
#   endif
#  endif
# endif
#endif

#if defined(__sparcv8)
# define PREDEF_CPU_SPARC CDETECT_MKVER(8, 0, 0)
#else
# if defined(__sparcv9) || defined(__sparc_v9__)
#  define PREDEF_CPU_SPARC CDETECT_MKVER(9, 0, 0)
# else
#  if defined(__sparc__) || defined(__sparc) || defined(sparc)
#   define PREDEF_CPU_SPARC CDETECT_MKVER(0, 0, 0)
#  endif
# endif
#endif

#if defined(__sh__)
# if defined(__SH5__)
#  define PREDEF_CPU_SUPERH CDETECT_MKVER(5, 0, 0)
# else
#  if defined(__SH4__)
#   define PREDEF_CPU_SUPERH CDETECT_MKVER(4, 0, 0)
#  else
#   if defined(__sh3__) || defined(__SH3__)
#    define PREDEF_CPU_SUPERH CDETECT_MKVER(3, 0, 0)
#   else
#    if defined(__sh2__)
#     define PREDEF_CPU_SUPERH CDETECT_MKVER(2, 0, 0)
#    else
#     if defined(__sh1__)
#      define PREDEF_CPU_SUPERH CDETECT_MKVER(1, 0, 0)
#     else
#      define PREDEF_CPU_SUPERH CDETECT_MKVER(0, 0, 0)
#     endif
#    endif
#   endif
#  endif
# endif
#endif

#if defined(_M_IX86)
# define PREDEF_CPU_X86 CDETECT_MKVER(_M_IX86 / 100, _M_IX86 % 100, 0)
#else
# if defined(__I86__)
#  define PREDEF_CPU_X86 CDETECT_MKVER(__I86__, 0, 0)
# else
#  if defined(i686) || defined(__i686) || defined(__i686__)
#   define PREDEF_CPU_X86 CDETECT_MKVER(6, 0, 0)
#  else
#   if defined(i586) || defined(__i586) || defined(__i586__)
#    define PREDEF_CPU_X86 CDETECT_MKVER(5, 0, 0)
#   else
#    if defined(i486) || defined(__i486) || defined(__i486__)
#     define PREDEF_CPU_X86 CDETECT_MKVER(4, 0, 0)
#    else
#     if defined(i386) || defined(__i386) || defined(__i386__)
#      define PREDEF_CPU_X86 CDETECT_MKVER(3, 0, 0)
#     else
#      if defined(_M_IX86) || defined(_X86_) || defined(__THW_INTEL)
#       define PREDEF_CPU_X86 CDETECT_MKVER(0, 0, 0)
#      endif
#     endif
#    endif
#   endif
#  endif
# endif
#endif

#if defined(_M_IA64)
# define PREDEF_CPU_IA64 CDETECT_MKVER((_M_IA64 % 64000) / 100, ((_M_IA64 % 64000) % 100), 0)
#else
# if defined(__ia64) || defined(__ia64__) || defined(_IA64) || defined(__IA64__)
#  define PREDEF_CPU_IA64 CDETECT_MKVER(0, 0, 0)
# endif
#endif

/*----------------------------------------------------------------------*/

cdetect_bool_t
chost_cpu_get(cdetect_string_t *name,
              unsigned int *version)
{
#if defined(PREDEF_CPU_ALPHA)
    *name = cdetect_string_format("alpha");
    *version = PREDEF_CPU_ALPHA;

#else
#if defined(PREDEF_CPU_AMD64)
    *name = cdetect_string_format("amd64");
    *version = PREDEF_CPU_AMD64;

#else
#if defined(PREDEF_CPU_ARM)
    *name = cdetect_string_format("arm");
    *version = PREDEF_CPU_ARM;

#else
#if defined(PREDEF_CPU_HPPA)
    *name = cdetect_string_format("hppa");
    *version = PREDEF_CPU_HPPA;

#else
#if defined(PREDEF_CPU_M68K)
    *name = cdetect_string_format("m68k");
    *version = PREDEF_CPU_M68K;

#else
#if defined(PREDEF_CPU_MIPS)
    *name = cdetect_string_format("mips");
    *version = PREDEF_CPU_MIPS;

#else
#if defined(PREDEF_CPU_POWERPC)
    *name = cdetect_string_format("powerpc");
    *version = PREDEF_CPU_POWERPC;

#else
#if defined(PREDEF_CPU_SPARC)
    *name = cdetect_string_format("sparc");
    *version = PREDEF_CPU_SPARC;

#else
#if defined(PREDEF_CPU_SUPERH)
    *name = cdetect_string_format("superh");
    *version = PREDEF_CPU_SUPERH;

#else
#if defined(PREDEF_CPU_X86)
    *name = cdetect_string_format("x86");
    *version = PREDEF_CPU_X86;

#else
#if defined(PREDEF_CPU_IA64)
    *name = cdetect_string_format("ia64");
    *version = PREDEF_CPU_IA64;

#else
    *name = 0;
    *version = 0;

#endif /* PREDEF_CPU_IA64 */
#endif /* PREDEF_CPU_X86 */
#endif /* PREDEF_CPU_SUPERH */
#endif /* PREDEF_CPU_SPARC */
#endif /* PREDEF_CPU_POWERPC */
#endif /* PREDEF_CPU_MIPS */
#endif /* PREDEF_CPU_M68K */
#endif /* PREDEF_CPU_HPPA */
#endif /* PREDEF_CPU_ARM */
#endif /* PREDEF_CPU_AMD64 */
#endif /* PREDEF_CPU_ALPHA */

    return (*name != 0) ? CDETECT_TRUE : CDETECT_FALSE;
}

/*************************************************************************
 *
 * main
 *
 ************************************************************************/

int main(int argc, char *argv[])
{
    cdetect_string_t compiler_name = 0;
    unsigned int compiler_version;
    cdetect_string_t kernel_name = 0;
    unsigned int kernel_version;
    cdetect_string_t cpu_name = 0;
    unsigned int cpu_version;

    cdetect_is_silent = CDETECT_TRUE;
    cdetect_is_nested = CDETECT_TRUE;
  
    config_begin();

    config_header_register(0);
  
    if (config_options(argc, argv)) {

        chost_compiler_get(&compiler_name, &compiler_version);
        chost_kernel_get(&kernel_name, &kernel_version);
        chost_cpu_get(&cpu_name, &cpu_version);
    
        printf("### %s:%x %s:%x %s:%x\n",
               (compiler_name) ? compiler_name->content : "",
               compiler_version,
               (kernel_name) ? kernel_name->content : "",
               kernel_version,
               (cpu_name) ? cpu_name->content : "",
               cpu_version);
    
        cdetect_string_destroy(cpu_name);
        cdetect_string_destroy(kernel_name);
        cdetect_string_destroy(compiler_name);
    }
    config_end();
  
    return 0;
}
