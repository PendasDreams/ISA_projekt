#!/bin/bash

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Define the server and port
SERVER="127.0.0.1"
PORT=1070

# Directory paths for downloading
SOURCE_DIR_DOWNLOAD="download"
DEST_DIR_DOWNLOAD="download_finish"

# Directory paths for uploading
SOURCE_DIR_UPLOAD="upload"
DEST_DIR_UPLOAD="upload_finish"

# Arrays to keep track of tests
PASSED_TESTS=()
FAILED_TESTS=()

# Check if directories exist
for DIR in $SOURCE_DIR_DOWNLOAD $DEST_DIR_DOWNLOAD $SOURCE_DIR_UPLOAD $DEST_DIR_UPLOAD; do
    if [[ ! -d $DIR ]]; then
        echo -e "${RED}Directory $DIR doesn't exist!${NC}"
        exit 1
    fi
done

# Function to perform tests
# Function to perform tests
# Function to perform tests
perform_test() {
    LOCAL_FILE=$1
    REMOTE_FILE=$2
    OPTIONS=$3
    OPERATION=$4

    if [[ $OPERATION == "upload" ]]; then
        COMMAND="echo \"$REMOTE_FILE\" | ./tftp-client -h $SERVER -p $PORT -f $LOCAL_FILE $OPTIONS"
    else
        COMMAND="./tftp-client -h $SERVER -p $PORT -f $LOCAL_FILE -t $REMOTE_FILE $OPTIONS"
    fi

    echo -e "${NC}Executing: $COMMAND${NC}"
    eval $COMMAND

    STATUS=$?
    echo -n "Testing $OPERATION of $(basename $LOCAL_FILE) $OPTIONS: "
    if [[ $STATUS -ne 0 ]]; then
        echo -e "${RED}FAILED${NC}"
        FAILED_TESTS+=("$(basename $LOCAL_FILE) $OPERATION $OPTIONS")
    else
        echo -e "${GREEN}PASSED${NC}"
        PASSED_TESTS+=("$(basename $LOCAL_FILE) $OPERATION $OPTIONS")
    fi
}



# Function to reset test directories for uploads
reset_upload_directories() {
    rm -rf $DEST_DIR_UPLOAD/*
}

# Upload tests without options
echo -e "\nProcessing directory: $SOURCE_DIR_UPLOAD without options"
reset_upload_directories
for LOCAL_FILE in $SOURCE_DIR_UPLOAD/*; do
    REMOTE_FILE="$DEST_DIR_UPLOAD/$(basename $LOCAL_FILE)"
    perform_test "$LOCAL_FILE" "$REMOTE_FILE" "" "upload"
done

sleep 1  # Give a moment before the next set of tests

# Upload tests with options
echo -e "\nProcessing directory: $SOURCE_DIR_UPLOAD with options"
reset_upload_directories
for LOCAL_FILE in $SOURCE_DIR_UPLOAD/*; do
    REMOTE_FILE="$DEST_DIR_UPLOAD/$(basename $LOCAL_FILE)"
    perform_test "$LOCAL_FILE" "$REMOTE_FILE" "--option blksize=800 --option timeout=3" "upload"
done

# ... [rest of the script unchanged]



# Compare directories
compare_directories() {
    DIR1=$1
    DIR2=$2
    echo -e "\nComparing directories $DIR1 and $DIR2"
    diff -r $DIR1 $DIR2
    DIFF_STATUS=$?
    if [ $DIFF_STATUS -ne 0 ]; then
        echo -e "${RED}The contents of $DIR1 and $DIR2 differ!${NC}"
        FAILED_TESTS+=("$DIR1 vs $DIR2 comparison")
    else
        echo -e "${GREEN}The contents of $DIR1 and $DIR2 are identical!${NC}"
        PASSED_TESTS+=("$DIR1 vs $DIR2 comparison")
    fi
}

compare_directories $SOURCE_DIR_UPLOAD $DEST_DIR_UPLOAD

# Print summary
echo -e "\n${GREEN}PASSED TESTS:${NC}"
for test in "${PASSED_TESTS[@]}"; do
    echo "  - $test"
done

echo -e "\n${RED}FAILED TESTS:${NC}"
for test in "${FAILED_TESTS[@]}"; do
    echo "  - $test"
done

# Exit with appropriate code
if [ ${#FAILED_TESTS[@]} -ne 0 ]; then
    exit 1
fi

echo -e "\nAll tests passed!"
