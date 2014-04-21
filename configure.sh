#!/bin/sh
##########################################################################
#
# http://cdetect.sourceforge.net/
#
# Copyright (C) 2005-2006 Bjorn Reese <breese@users.sourceforge.net>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
# MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS AND
# CONTRIBUTORS ACCEPT NO RESPONSIBILITY IN ANY CONCEIVABLE MANNER.
#
##########################################################################

SOURCE="config.c"
DEPEND="${SOURCE} cdetect/*.c"
COMMAND="config"

UPDATE=0

# FIXME: fails if $0 does not contain /
MYDIR="`echo $0 | sed -e 's/\(.*\)\/configure\.sh/\1/'`"
if [ "x${MYDIR}" = "x" ]; then
    MYDIR="."
fi
MYCFLAGS="-I${MYDIR}"

# Extract options for this script only
if [ "x$1" = "x--debug" ]; then
    shift
    MYCFLAGS="-g -Wall ${MYCFLAGS}"
fi

ARGUMENTS=$*

COMPILER=""

# Scan the remaining options
while [ "x$*" != "x" ]; do
    case $1 in
	-c | --compiler)
	    shift
	    if [ "x$1" = "x=" ]; then
		shift
	    fi
	    COMPILER=$1
	    ;;
	    
	--compiler=*)
	    COMPILER=`echo $1 | sed -e 's/.*=\(.*\)/\1/'`
	    ;;
	    
	--cflags)
	    shift
	    CFLAGS="${CFLAGS} $1"
	    ;;
	    
	--cflags=*)
	    CFLAGS="${CFLAGS} `echo $1 | sed -e 's/.*=\(.*\)/\1/'`"
	    ;;
	    
	*)
	    ;;
    esac
    shift
done

# Determine compiler
if [ "x${COMPILER}" = "x" ]; then
    if [ "x${CC}" = "x" ]; then
	COMPILER="cc"
    else
	COMPILER=${CC}
    fi
fi

# Determine dependencies
if [ -x ${COMMAND} ]; then
    if [ "x`ls -t ${COMMAND} ${DEPEND} 2>/dev/null | head -1`" != "x${COMMAND}" ]; then
        UPDATE=1
    fi
else
    UPDATE=1
fi

# Build cDetect executable if needed
if [ ${UPDATE} -ne 0 ]; then
    echo "Updating ${COMMAND}"
    rm -f ${COMMAND}
    ${COMPILER} ${MYCFLAGS} ${CFLAGS} ${MYDIR}/${SOURCE} -o ${COMMAND}
    if [ $? -ne 0 ]; then
	exit 1
    fi
fi

# Execute cDetect if it exists
if [ -x ${COMMAND} ]; then
    ./${COMMAND} ${ARGUMENTS}
else
    echo "Error: ${COMMAND} not found"
    exit 1
fi
