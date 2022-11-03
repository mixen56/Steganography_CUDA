#!/bin/sh

# USE: $0 Size_in_kb output_file_name

OUTPUT_FILE=${2:-generated_file}
echo "Output file = $OUTPUT_FILE"
rm $OUTPUT_FILE 2> /dev/null

SIZE=${1:-1}    # 1KB default
echo "Size = $SIZE kb"
BYTES=$(( $SIZE * 1024))

HEAD=100

if [ $SIZE -gt 1000 ]; then
    HEAD=1000
elif [ $SIZE -gt 10000 ]; then
    HEAD=10000
elif [ $SIZE -gt 100000 ]; then
    HEAD=100000
fi

cur_size=0
while [ $cur_size -lt $BYTES ]; do
    cat /dev/urandom | tr -dc '[:alpha:]' | fold -w 80 | head -n $HEAD >> $OUTPUT_FILE
    cur_size=`du -b $OUTPUT_FILE | awk '{print $1}'`
    printf "Size of %s = %d\r" "$OUTPUT_FILE" "$cur_size"
done
