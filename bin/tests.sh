#!/bin/bash

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Define the server and port
SERVER="127.0.0.1"
PORT=1070

# Directory paths for downloading from server to client
ROOT_DIRPATH_SERVER="root_dirpath_server" # Assuming this is the server's root directory.
CLIENT_SERVER_PATH="root_dirpath_client"   # Client directory to which files are downloaded.


# Arrays to keep track of tests
PASSED_TESTS=()
FAILED_TESTS=()


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

run_tftp_client() {
    echo "Running: ./tftp-client $@"
    ./tftp-client $@
}



# Download files from ROOT_DIRPATH_SERVER to CLIENT_SERVER_PATH
echo -e "\nProcessing directory: $ROOT_DIRPATH_SERVER"
for SERVER_FILE in $ROOT_DIRPATH_SERVER/*; do
    FILE_NAME=$(basename $SERVER_FILE)
    CLIENT_FILE="$CLIENT_SERVER_PATH/$FILE_NAME"
    echo -n "Testing download of $FILE_NAME from server to client: "
    run_tftp_client -h $SERVER -p $PORT -f $FILE_NAME -t $CLIENT_FILE
    # Check success and store results
    if [[ $? -ne 0 ]]; then
        echo -e "${RED}FAILED${NC}"
        FAILED_TESTS+=("$FILE_NAME download from server")
    else
        echo -e "${GREEN}PASSED${NC}"
        PASSED_TESTS+=("$FILE_NAME download from server")
    fi
done

compare_directories $ROOT_DIRPATH_SERVER $CLIENT_SERVER_PATH


# Upload files from CLIENT_SERVER_PATH back to ROOT_DIRPATH_SERVER
echo -e "\nProcessing directory: $CLIENT_SERVER_PATH"
for CLIENT_FILE in $CLIENT_SERVER_PATH/*; do
    FILE_NAME=$(basename $CLIENT_FILE)
    echo -n "Testing upload of $FILE_NAME from client to server: "
    echo "$CLIENT_FILE" | run_tftp_client -h $SERVER -p $PORT -t $FILE_NAME
    # Check success and store results
    if [[ $? -ne 0 ]]; then
        echo -e "${RED}FAILED${NC}"
        FAILED_TESTS+=("$FILE_NAME upload to server")
    else
        echo -e "${GREEN}PASSED${NC}"
        PASSED_TESTS+=("$FILE_NAME upload to server")
    fi
done



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