#!/bin/bash

set -ex

if [ "x$TESTGCC" = "x" ]; then
  TESTGCC=$(readlink -f $(which gcc))
fi

if [ "x$TESTGCC" = "x" ]; then
  echo "WARNING: gcc not found. Cannot execute test." 2>&1
  exit 1;
fi

if [ "x$TESTROOT" = "x" ]; then
  echo "TESTROOT pointintg to top level sources is not set. that is fatal"
  exit 2;
fi

if [ "x$TESTJAVA" = "x" ]; then
  TESTJAVA=$(dirname $(dirname $(readlink -f $(which java))))
fi

JDK_TOPDIR="${TESTROOT}/.."
JAVA="${TESTJAVA}/bin/java"
TEST_ENV_SH="${JDK_TOPDIR}/../hotspot/test/test_env.sh"

ls -l "${TEST_ENV_SH}"
set +e
  . "${TEST_ENV_SH}"
set -e

"${TESTGCC}" \
    -fPIC \
    -shared \
    ${CFLAGBITS} \
    -o ${TESTCLASSES}/libNativeThread.so \
    -I${JDK_TOPDIR}/src/share/bin \
    -I${JDK_TOPDIR}/src/share/javavm/export \
    -I${JDK_TOPDIR}/src/macosx/javavm/export \
    -I${JDK_TOPDIR}/src/solaris/bin \
    ${TESTSRC}/libNativeThread.c

"${JAVA}" -Dtest.jdk=${TESTJAVA} \
          -Dtest.nativepath=${TESTCLASSES} \
          -Djava.library.path=${TESTCLASSES} \
          -cp ${TESTCLASSPATH}:${TESTCLASSES} B8312065
