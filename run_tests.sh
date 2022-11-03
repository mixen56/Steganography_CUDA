#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

die(){
    echo -e "$RED$1$NC"
    exit 1
}

FILE_LIST=( "files/CASE_1.txt" "files/CASE_1_norem.txt" "files/CASE_2.txt" "files/CASE_2_norem.txt" )
INPUT_VIDEO=test_640x360.mp4
OUTPUT_VIDEO=Hided_test_640x360.mp4.avi

echo -e "${BLUE}Input Video:${NC} $INPUT_VIDEO"
echo

for file in ${FILE_LIST[@]}; do
    echo "------------------------------------------------------------"
    echo -e "${BLUE}HIDE${NC}: $file"
    ./Hide $INPUT_VIDEO $file 50 1 || die "Cant hide $file"

    echo -e "${BLUE}UNHIDE${NC}"
    ./Dishide $OUTPUT_VIDEO 1      || die "Cant unhide from $OUTPUT_VIDEO"

    output_file="Hided_`echo $file | cut -d '/' -f 2`"
    echo -e "${BLUE}Compare files${NC}"
    diff $file $output_file || die "Files are different"

    echo -e "Case $file: ${GREEN}OK${NC}"
    echo "------------------------------------------------------------"
    echo
done

echo -e "ALL TESTS: ${GREEN}PASS${NC}"
