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

# @test @(#)TestClassRedefine.sh
# @summary Test class redefine support in CodeRevive
# @requires (os.family == "linux") & (os.arch == "amd64")
# @run shell TestClassRedefine.sh

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
cp ${TESTSRC}${FS}agent-classes${FS}* .
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
${JAR} -cvfm RedefineAgent.jar manifest.inf AgentMain.class TestTransformer.class
check_ret $? "JAR RedefineAgent.jar"

function check_msg() {
	local LOG_FILE=$1
	local MSG=$2
	local COUNT=$3

	local CNT=`cat ${LOG_FILE} | grep "${MSG}" | wc -l`
	if [ ${CNT} -ne ${COUNT} ]
	then
		echo "Message '${MSG}' should appear ${COUNT} times, but appears ${CNT} times."
		exit -1
	fi
}

function generate_merged_csa() {
	# save generated test.csa for redefined TestRedefine.add(I)I
	# this functioni will generate code for original TestRedefine.add(I)I, and merge the two versions.

	echo "< Generating original code version >"
	${JAVA} -XX:+Inline -XX:CompileOnly=TestRedefine -XX:CodeReviveOptions=save,file=test2.csa,log=save=trace TestRedefine

	echo "< Merging code versions >"
	echo "test.csa" > input.list
	echo "test2.csa" >> input.list
	${JAVA} -XX:CodeReviveOptions=merge,input_list_file=input.list,file=test_merged.csa -version

	${CP} -f test_merged.csa test.csa
}

function do_test() {
	local TYPE=$1

	# run test class
	${JAVA} -XX:+Inline -XX:CompileOnly=TestRedefine -XX:CodeReviveOptions=${TYPE},file=test.csa,log=${TYPE}=info,archive=trace Test 1>${TYPE}.log 2>&1 &
	TESTPID=$!

	# attach agent to redefine class
	sleep 5
	${JAVA} -cp ${CLASSPATH} AttachAgent ${TESTPID} RedefineAgent.jar 1>attach.log 2&>1
	RET2=$?

	wait ${TESTPID}
	RET1=$?

	cat ${TYPE}.log

	if [ ${RET1} -ne 0 ] || [ ${RET2} -ne 0 ]
	then
		echo "Run test ${TYPE} failed."
		exit -1
	fi

	# check result
	check_msg ${TYPE}.log "Original TestRedefine.print()." 2

	check_msg ${TYPE}.log "Redefine caused set redefine epoch of TestRedefine to be invalid." 1
	check_msg ${TYPE}.log "Subclass SubTestRedefine is invalid, stop searching." 1
	check_msg ${TYPE}.log "Interface RedefineInterface is redefined, set global redefine epoch to 3." 1

	if [ x"${TYPE}" == "xsave" ]
	then
		check_msg ${TYPE}.log "Identity for class TestRedefine may be deprecated. (Epoch: 0 < 3)" 1
		check_msg ${TYPE}.log "Identity for class java.lang.Object may be deprecated. (Epoch: 0 < 3)" 1
		check_msg ${TYPE}.log "Identity for class RedefineInterface may be deprecated. (Epoch: 0 < 3)" 1
		check_msg ${TYPE}.log "java.lang.Object: Set redefine epoch to 3." 1
		check_msg ${TYPE}.log "RedefineInterface: Set redefine epoch to 3." 1
		check_msg ${TYPE}.log "TestRedefine: Set redefine epoch to 3." 1
		check_msg ${TYPE}.log "CodeRevive Identity for class TestRedefine is " 1
		check_msg ${TYPE}.log "CodeRevive Identity for class RedefineInterface is " 1
	        check_msg ${TYPE}.log "Redefine caused set redefine epoch of RedefineInterface to be invalid." 1
	fi

	if [ x"${TYPE}" == "xrestore" ]
	then
		check_msg ${TYPE}.log "Identity for class TestRedefine may be deprecated. (Epoch: 0 < 1)" 1
		check_msg ${TYPE}.log "Identity for class java.lang.Object may be deprecated. (Epoch: 0 < 1)" 1
		check_msg ${TYPE}.log "Identity for class RedefineInterface may be deprecated. (Epoch: 0 < 1)" 1
		check_msg ${TYPE}.log "Identity for class TestRedefine may be deprecated. (Epoch: 0 < 3)" 1
		check_msg ${TYPE}.log "Identity for class RedefineInterface may be deprecated. (Epoch: 0 < 3)" 1
		check_msg ${TYPE}.log "java.lang.Object: Set redefine epoch to 1." 1
		check_msg ${TYPE}.log "RedefineInterface: Set redefine epoch to 1." 1
		check_msg ${TYPE}.log "TestRedefine: Set redefine epoch to 1." 1
		check_msg ${TYPE}.log "CodeRevive Identity for class TestRedefine is " 2
		check_msg ${TYPE}.log "CodeRevive Identity for class RedefineInterface is " 2
		check_msg ${TYPE}.log "Redefine caused set redefine epoch of RedefineInterface to be invalid." 2
		check_msg ${TYPE}.log "revive success: TestRedefine.add(I)I" 2

	fi
}

echo "do_test save"
do_test save

echo "generate_merged_csa"
generate_merged_csa

echo "do_test restore"
do_test restore

exit 0
