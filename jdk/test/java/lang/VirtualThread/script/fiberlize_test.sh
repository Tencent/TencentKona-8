#
# Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

# input is java file list
# output is replaced java file with annotated main wrapper trigger main in virtual thread
dirpath=`dirname "$0"`
for filename in "$@"
do
    newfilename="$filename.new"
    python $dirpath/fiberlize_test.py $filename > $newfilename
    if [ $? -eq 0 ]; then
      echo $filename " modified"
      mv $newfilename $filename
    else
      echo $filename " skipped"
      rm $newfilename
    fi
done
