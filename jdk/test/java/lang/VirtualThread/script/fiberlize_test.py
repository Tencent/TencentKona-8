# Copyright (C) 2021, 2023, THL A29 Limited, a Tencent company. All rights reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.

import sys
import re

newMainCode = """
  public static void main(final String[] args) {
    Runnable _target = new Runnable() {
      public void run() {
          try {
              _main(args);
          } catch (Throwable t) {
             System.out.println(t);
             System.exit(121);
          }
      }
    };
    Thread vt = Thread.builder().virtual().task(_target).name("ForkJoinPool-1-worker-1").build();
    vt.start();
    try {
      vt.join();
    } catch (Throwable t) {
      System.out.println(t);
      System.exit(122);
    }
  }
"""

newMainCode2 = """
  public static void main(final String... args) throws Exception {
    Runnable _target = new Runnable() {
      public void run() {
          try {
              _main(args);
          } catch (Throwable t) {
             System.out.println(t);
             System.exit(121);
          }
      }
    };
    Thread vt = Thread.builder().virtual().task(_target).name("ForkJoinPool-1-worker-1").build();
    vt.start();
    vt.join();
  }
"""

# file not test and need skip
skip_modify_set = {
"SubclassAcrossPackage", "CipherTestUtils", "CipherTest"
}

# fail test skip now
skip_file_list = {
"hotspot/test/runtime/7194254/Test7194254.java",
"jdk/test/jdk/jfr/api/consumer/TestHiddenMethod.java"
}

test_dir_prefix = {"jdk/test", "hotspot/test", "langtools/test", "nashorn/test"}

def checkAddInstall(line, classname):
    if classname in line:
        return True
    return False

filename = sys.argv[1]
dotIndex = filename.rfind('.')
nameindex = filename.rfind('/')
classname = filename[nameindex+1:dotIndex]
if classname in skip_modify_set or "VirtualThread" in filename:
    exit(1)
# chop file prefix and match in skip_file_list
for prefix in test_dir_prefix:
    index = filename.find(prefix)
    if index != -1 :
        canonical_name = filename[index : ]
        if canonical_name in skip_file_list:
            exit(1)

checkingClassFileInstaller = False
printClassFileInstaller = False
f = open(filename)
line = f.readline()
while line:
    if ("public static void main" in line or "static public void main" in line or "public static final void main" in line) and ("println" not in line) and ("\"" not in line) and ("PKCS11Test" not in line):
        if (not re.match(".*[\t| ]+main[\t| ]*\(.*", line)) or ("//" in line):
            print line,
        elif ("String..." in line):
            print newMainCode2,
            print line.replace("main", "_main"),
        else:
            print newMainCode,
            print line.replace("main", "_main"),
    elif "@compile/fail/ref=" in line:
       # skip all the compile fail tests
       exit(1)
    elif "@run main ClassFileInstaller" in line:
        # read unitl empty line line with @
        checkingClassFileInstaller = True
        printClassFileInstaller = checkAddInstall(line, classname)
        print line,
    else:
        if checkingClassFileInstaller:
            if "@" in line or "*/" in line:
                checkingClassFileInstaller = False
            else:
                printClassFileInstaller = checkAddInstall(line, classname)
        print line,

    if printClassFileInstaller:
        # split and get string contains classname
        # insert *   classname$1 anonymous class name
        splits = line.split()
        for word in splits:
            if classname in word:
                print "* " + word + "$1"
                break
        printClassFileInstaller = False
        checkingClassFileInstaller = False
    line = f.readline()
f.close();
