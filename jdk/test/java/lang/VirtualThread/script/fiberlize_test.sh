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
