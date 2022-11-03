#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

die(){
    echo -e "$RED$1$NC"
    exit 1
}

FILE_LIST=( "generated_file_1" "generated_file_2" )
INPUT_VIDEO_LIST=( "VIDEO_20MB.mp4" "VIDEO_100MB.mp4" )

echo -e "${BLUE}Input Video:${NC} $INPUT_VIDEO"
echo

for i in 0 1; do
    echo "------------------------------------------------------------"
    file=${FILE_LIST[$i]}
    output_file="Hided_`echo $file`"
    video=${INPUT_VIDEO_LIST[$i]}
    output_video="Hided_`echo $video`.avi"

    echo -e "${BLUE}HIDE${NC}: $file"
    ./Hide $video $file 50 1    || die "Cant hide $file"

    echo -e "${BLUE}UNHIDE${NC}"
    ./Dishide $output_video 1   || die "Cant unhide from $output_video"

    echo -e "${BLUE}Compare files${NC}"
    diff $file $output_file     || die "Files are different"

    echo -e "Case $file: ${GREEN}OK${NC}"
    echo "------------------------------------------------------------"
    echo
done

echo -e "ALL TESTS: ${GREEN}PASS${NC}"
