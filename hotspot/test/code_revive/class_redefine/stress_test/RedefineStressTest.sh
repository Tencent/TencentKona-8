#
# Copyright (C) 2023, Tencent. All rights reserved.
# DO NOT ALTER OR REMOVE NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation. Tencent designates
# this particular file as subject to the "Classpath" exception as provided
# by Oracle in the LICENSE file that accompanied this code.
#
# This code is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License version 2 for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

# @test @(#)RedefineStressTest.sh
# @summary StressTest class redefine support in CodeRevive
# @requires (os.family == "linux") & (os.arch == "amd64")
# @run shell RedefineStressTest.sh

if [ "${TESTSRC}" = "" ]
then
  TESTSRC=${PWD}
  echo "TESTSRC not set.  Using "${TESTSRC}" as default"
fi
echo "TESTSRC=${TESTSRC}"
## Adding common setup Variables for running shell tests.
. ${TESTSRC}/../../../test_env.sh

TEST_TIME_SECS=30

JAVA=${TESTJAVA}${FS}bin${FS}java
JAVAC=${COMPILEJAVA}${FS}bin${FS}javac
JAR=${COMPILEJAVA}${FS}bin${FS}jar

CLASSPATH=${CLASSPATH}${FS}${COMPILEJAVA}${FS}lib${FS}tools.jar${PS}

REDEFINED_CLASS=TimedExecutor
REDEFINED_SRC=${TESTSRC}${FS}test-classes${FS}${REDEFINED_CLASS}.java

# prepare original test classes
cp ${TESTSRC}${FS}test-classes${FS}* .
cp ${TESTSRC}${FS}agent-classes${FS}* .
cp ${TESTSRC}${FS}..${FS}function_test${FS}agent-classes${FS}AttachAgent.java .

mkdir -p redefined-classes
mkdir -p generated-classes

function check_ret() {
	local RV=$1
	local STR=$2

	if [ ${RV} -eq 0 ]
	then
		return
	fi

	echo "Failed to run ${STR}."
	exit -1
}

echo ${CLASSPATH}

# compile java sources
${JAVAC} -cp ${CLASSPATH} *.java
check_ret $? "JAVAC *.java"

# package redefine agent
${JAR} -cvfm RedefineAgent.jar manifest.inf AgentMain.class
check_ret $? "JAR RedefineAgent.jar"

# generate redefined classes
CLASS_NUM=8
rm -rf input.list
cd generated-classes
for i in `seq 0 $((${CLASS_NUM}-1))`
do
	sed "s/TimedExecutor Version 0/TimedExecutor Version ${i}/g" ${REDEFINED_SRC} &> ${REDEFINED_CLASS}.java
	${JAVAC} ${REDEFINED_CLASS}.java
        ${JAVA} -XX:CompileOnly=TimedExecutor.add1,TimedExecutor.add2 -XX:CodeReviveOptions=save,log=save=trace,file=../redefine_${i}.csa TimedExecutor 2 &> ../save_${i}.log
	echo "redefine_${i}.csa" >> ../input.list

	mv ${REDEFINED_CLASS}.class ${REDEFINED_CLASS}.${i}.class
done
cd ..

# generate merged csa file
${JAVA} -XX:CodeReviveOptions=merge,input_list_file=input.list,file=test_redefine.csa -version

# restore
${JAVA} -XX:+PrintCompilation -XX:CompileOnly=TimedExecutor.add1,TimedExecutor.add2 -XX:CodeReviveOptions=restore,log=restore=trace,file=test_redefine.csa,verify_redefined_identity,make_revive_fail_at_nmethod TimedExecutor ${TEST_TIME_SECS} 1>restore.log 2>&1 &

RESTORE_PID=$!

while :
do
	CLASS_INDEX=$((${RANDOM} % ${CLASS_NUM}))
	cp generated-classes/${REDEFINED_CLASS}.${CLASS_INDEX}.class redefined-classes/${REDEFINED_CLASS}.class
	${JAVA} -cp ${CLASSPATH} AttachAgent ${RESTORE_PID} RedefineAgent.jar 1>attach.log 2&>1
	if [ $? -ne 0 ]
	then
		break;
	fi
done

wait ${RESTORE_PID}
if [ $? -ne 0 ]
then
	echo "Class Redefine Stress Test failed."
	return -1
fi

exit 0
