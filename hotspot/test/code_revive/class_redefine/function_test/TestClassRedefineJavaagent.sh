#
# Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
# DO NOT ALTER OR REMOVE NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation. THL A29 Limited designates
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

# @test @(#)TestClassRedefineJavaagent.sh
# @summary Test CodeRevive can work when  -javaagent triggers class redefine
# @run shell TestClassRedefineJavaagent.sh

if [ "${TESTSRC}" = "" ]
then
  TESTSRC=${PWD}
  echo "TESTSRC not set.  Using "${TESTSRC}" as default"
fi
echo "TESTSRC=${TESTSRC}"
## Adding common setup Variables for running shell tests.
. ${TESTSRC}/../../../test_env.sh

JAVA=${TESTJAVA}${FS}bin${FS}java
JAVAC=${COMPILEJAVA}${FS}bin${FS}javac
JAR=${COMPILEJAVA}${FS}bin${FS}jar

CLASSPATH=${CLASSPATH}${FS}${COMPILEJAVA}${FS}lib${FS}tools.jar${PS}

# prepare original test classes
cp ${TESTSRC}${FS}test-classes${FS}* .
cp ${TESTSRC}${FS}agent-classes-javaagent${FS}* .
cp -r ${TESTSRC}${FS}redefined-classes .

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

cd redefined-classes
${JAVAC} -cp . *.java
check_ret $? "JAVAC redefined-class/*.java"
cd ..

# package redefine agent
${JAR} -cvfm RedefineAgent.jar manifest.inf PreMain.class
check_ret $? "JAR RedefineAgent.jar"

function check_msg() {
	local LOG_FILE=$1
	local MSG=$2
	local COUNT=$3
    cat ${LOG_FILE}
	local CNT=`cat ${LOG_FILE} | grep "${MSG}" | wc -l`
	if [ ${CNT} -ne ${COUNT} ]
	then
		echo "Message '${MSG}' should appear ${COUNT} times, but appears ${CNT} times."
		exit -1
	fi
}

function generate_merged_csa() {
	local AGENT=$1

	echo "< Generating original code version >"
	if [ x"${AGENT}" == "xtrue" ]
	then
		${JAVA} -XX:+Inline -javaagent:RedefineAgent.jar -cp ${CLASSPATH} -XX:CompileOnly=TestRedefine -XX:CodeReviveOptions=save,file=test2.csa,log=save=trace TestRedefine
	else
		${JAVA} -XX:+Inline -XX:CompileOnly=TestRedefine -XX:CodeReviveOptions=save,file=test2.csa,log=save=trace TestRedefine
	fi
	echo "< Merging code versions >"
	echo "test.csa" > input.list
	echo "test2.csa" >> input.list
	${JAVA} -XX:CodeReviveOptions=merge,input_list_file=input.list,file=test_merged.csa -version

	${CP} -f test_merged.csa test.csa
}

function do_test() {
	local TYPE=$1
	local AGENT=$2

	# run test class
	if [ x"${AGENT}" == "xtrue" ]
	then
		${JAVA} -XX:+Inline -javaagent:RedefineAgent.jar -cp ${CLASSPATH} -XX:CompileOnly=TestRedefine -XX:CodeReviveOptions=${TYPE},file=test.csa,log=${TYPE}=info,archive=trace TestRedefine 1>${TYPE}.log 2>&1 &
	else
		${JAVA} -XX:+Inline -XX:CompileOnly=TestRedefine -XX:CodeReviveOptions=${TYPE},file=test.csa,log=${TYPE}=info,archive=trace TestRedefine 1>${TYPE}.log 2>&1 &
	fi

	TESTPID=$!

	wait ${TESTPID}
	RET1=$?
	cat ${TYPE}.log

	if [ ${RET1} -ne 0 ]
	then
		echo "Run test ${TYPE} failed."
		exit -1
	fi

	if [ x"${AGENT}" == "xtrue" ]
	then
		check_msg ${TYPE}.log "Redefine caused set redefine epoch of java.lang.Object to be invalid." 1
	fi

	if [ x"${TYPE}" == "xsave" ]
	then
		check_msg ${TYPE}.log "Succeed to save CSA file test.csa" 1
	fi

	if [ x"${TYPE}" == "xrestore" ]
	then
		check_msg ${TYPE}.log "Succeed to load CSA file test.csa" 1
		if [ x"${AGENT}" == "xtrue" ]
        then
			check_msg ${TYPE}.log "revive success: TestRedefine.add(I)I" 1
		fi
	fi
}

echo "Test1: save without agent, restore with agent"

do_test save false
generate_merged_csa false
do_test restore true

echo "Test2: save with agent, restore without agent"

do_test save true
generate_merged_csa true
do_test restore false

echo "Test3: save with agent, restore with agent"

do_test save true
generate_merged_csa true
do_test restore true

exit 0
