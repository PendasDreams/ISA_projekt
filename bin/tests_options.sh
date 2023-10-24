#!/bin/bash

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Define the server and port
SERVER="127.0.0.1"
PORT=1070

# Directory paths for downloading from server to client
ROOT_DIRPATH_SERVER="root_dirpath_server"
CLIENT_SERVER_PATH="root_dirpath_client"

# Arrays to keep track of tests
PASSED_TESTS=()
FAILED_TESTS=()

# Options for testing
BLKSIZES=("800" "1600" "3200")
TIMEOUTS=("3" "5" "7")
TSIZE="0"

run_tftp_client() {
    echo "Running: ./tftp-client $@"
    ./tftp-client $@
}

compare_directories() {
    echo -e "\nComparing directories $1 and $2"
    diff -r $1 $2
    DIFF_STATUS=$?
    if [ $DIFF_STATUS -ne 0 ]; then
        echo -e "${RED}The contents of $1 and $2 differ!${NC}"
        FAILED_TESTS+=("Directory comparison of $1 and $2")
    else
        echo -e "${GREEN}The contents of $1 and $2 are identical!${NC}"
        PASSED_TESTS+=("Directory comparison of $1 and $2")
    fi
}

# Check if directories exist
for DIR in $ROOT_DIRPATH_SERVER $CLIENT_SERVER_PATH; do
    if [[ ! -d $DIR ]]; then
        echo -e "${RED}Directory $DIR doesn't exist!${NC}"
        exit 1
    fi
done

# Download and Upload tests with combinations of options
for BLKSIZE in "${BLKSIZES[@]}"; do
    for TIMEOUT in "${TIMEOUTS[@]}"; do
        OPTIONS="--option blksize=$BLKSIZE --option timeout=$TIMEOUT --option tsize=$TSIZE"

        # Download
        echo -e "\nProcessing download with options: $OPTIONS"
        for SERVER_FILE in $ROOT_DIRPATH_SERVER/*; do
            FILE_NAME=$(basename $SERVER_FILE)
            CLIENT_FILE="$CLIENT_SERVER_PATH/$FILE_NAME"
            run_tftp_client -h $SERVER -p $PORT -f $FILE_NAME -t $CLIENT_FILE $OPTIONS
            # Check success and store results
            if [[ $? -ne 0 ]]; then
                FAILED_TESTS+=("$FILE_NAME download from server with options: $OPTIONS")
            else
                PASSED_TESTS+=("$FILE_NAME download from server with options: $OPTIONS")
            fi
        done

        

        # Upload
        echo -e "\nProcessing upload with options: $OPTIONS"
        for CLIENT_FILE in $CLIENT_SERVER_PATH/*; do
            FILE_NAME=$(basename $CLIENT_FILE)
            echo "$CLIENT_FILE" | run_tftp_client -h $SERVER -p $PORT -t $FILE_NAME $OPTIONS
            # Check success and store results
            if [[ $? -ne 0 ]]; then
                FAILED_TESTS+=("$FILE_NAME upload to server with options: $OPTIONS")
            else
                PASSED_TESTS+=("$FILE_NAME upload to server with options: $OPTIONS")
            fi
        done
    done
done

# Comparisons
compare_directories $ROOT_DIRPATH_SERVER $CLIENT_SERVER_PATH

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
