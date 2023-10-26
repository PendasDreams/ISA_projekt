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
    ./tftp-client "$@"  # Enclose "$@" in double quotes
}



compare_directories() {
    echo -e "\nComparing directories $1 and $2"
    
    # Capture the output of diff command in a variable and remove null bytes
    DIFF_OUTPUT=$(diff -r $1 $2 | tr -d '\0')
    DIFF_STATUS=$?
    
    if [ $DIFF_STATUS -ne 0 ]; then
        echo -e "${RED}The contents of $1 and $2 differ!${NC}"
        
        # Extract and print differing filenames from the diff output
        echo "$DIFF_OUTPUT" | grep -E 'Only in' | awk -F': ' '{print $2}'
        
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
        # Options are passed in the desired format now
        OPTIONS="--option \"blksize $BLKSIZE\" --option \"timeout $TIMEOUT\" --option \"tsize $TSIZE\""

# Download
echo -e "\nProcessing download with options: $OPTIONS"
for SERVER_FILE in $ROOT_DIRPATH_SERVER/*; do
    FILE_NAME=$(basename $SERVER_FILE)
    CLIENT_FILE="$CLIENT_SERVER_PATH/$FILE_NAME"
    eval run_tftp_client -h $SERVER -p $PORT -f $FILE_NAME -t $CLIENT_FILE $OPTIONS
    # Check success and store results
    if [[ $? -ne 0 ]]; then
        FAILED_TESTS+=("$FILE_NAME download from server with options: $OPTIONS")
    else
        PASSED_TESTS+=("$FILE_NAME download from server with options: $OPTIONS")
    fi
done

        # Compare directories after Download
        compare_directories $ROOT_DIRPATH_SERVER $CLIENT_SERVER_PATH

# Upload
echo -e "\nProcessing upload with options: $OPTIONS"
for CLIENT_FILE in $CLIENT_SERVER_PATH/*; do
    FILE_NAME=$(basename "$CLIENT_FILE")
    
    # Create a temporary file containing the file path
    TEMP_FILE=$(mktemp)
    echo "$CLIENT_SERVER_PATH/$FILE_NAME" > "$TEMP_FILE"
    
    # Construct the command for upload
    UPLOAD_COMMAND="./tftp-client -h $SERVER -p $PORT -t \"$FILE_NAME\" $OPTIONS < \"$TEMP_FILE\""
    echo "Running upload command: $UPLOAD_COMMAND"
    
    # Execute the upload command
    eval $UPLOAD_COMMAND
    
    # Check success and store results
    if [[ $? -ne 0 ]]; then
        FAILED_TESTS+=("$FILE_NAME upload to server with options: $OPTIONS")
    else
        PASSED_TESTS+=("$FILE_NAME upload to server with options: $OPTIONS")
    fi
    
    # Clean up the temporary file
    rm "$TEMP_FILE"
done



        # Compare directories after Upload
        compare_directories $ROOT_DIRPATH_SERVER $CLIENT_SERVER_PATH
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
