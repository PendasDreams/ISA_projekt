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

run_tftp_client() {
    echo "Running: ./tftp-client $@"
    ./tftp-client $@
}

# echo "Processing directory: $SOURCE_DIR_DOWNLOAD"
# # Iterate over each file in the download directory
# for LOCAL_FILE in $SOURCE_DIR_DOWNLOAD/*; do
#     FILE_NAME=$(basename $LOCAL_FILE)
#     REMOTE_FILE="$DEST_DIR_DOWNLOAD/$FILE_NAME"
#     echo -n "Testing download of $FILE_NAME without any optional parameters: "
#     run_tftp_client -h $SERVER -p $PORT -f $LOCAL_FILE -t $REMOTE_FILE
#     # ... [zbytek kódu zůstává stejný]
# done

# echo -e "\nProcessing directory: $SOURCE_DIR_UPLOAD"
# # Iterate over each file in the upload directory
# for LOCAL_FILE in $SOURCE_DIR_UPLOAD/*; do
#     FILE_NAME=$(basename $LOCAL_FILE)
#     REMOTE_FILE="$DEST_DIR_UPLOAD/$FILE_NAME"
#     echo -n "Testing upload of $FILE_NAME: "
#     # Zde upravujeme přímý vstup do tftp-client
#     echo "Running: echo $REMOTE_FILE | ./tftp-client -h $SERVER -p $PORT -f $LOCAL_FILE -t $REMOTE_FILE"
#     echo $REMOTE_FILE | ./tftp-client -h $SERVER -p $PORT -f $LOCAL_FILE -t $REMOTE_FILE
#     # ... [zbytek kódu zůstává stejný]
# done

# Testování s parametrem --option
echo -e "\nProcessing directory with options: $SOURCE_DIR_DOWNLOAD"
for LOCAL_FILE in $SOURCE_DIR_DOWNLOAD/*; do
    FILE_NAME=$(basename $LOCAL_FILE)
    REMOTE_FILE="$DEST_DIR_DOWNLOAD/$FILE_NAME"
    echo -n "Testing download of $FILE_NAME with blksize option: "
    run_tftp_client -h $SERVER -p $PORT -f $LOCAL_FILE -t $REMOTE_FILE --option blksize=8000
    if [[ $? -ne 0 ]]; then
        echo -e "${RED}FAILED${NC}"
        FAILED_TESTS+=("$FILE_NAME download with blksize")
    else
        echo -e "${GREEN}PASSED${NC}"
        PASSED_TESTS+=("$FILE_NAME download with blksize")
            sleep 0.2  # Pauza 0.2 sekundy mezi testy

    fi

    echo -n "Testing download of $FILE_NAME with timeout option: "
    run_tftp_client -h $SERVER -p $PORT -f $LOCAL_FILE -t $REMOTE_FILE --option timeout=5
    if [[ $? -ne 0 ]]; then
        echo -e "${RED}FAILED${NC}"
        FAILED_TESTS+=("$FILE_NAME download with timeout")
    else
        echo -e "${GREEN}PASSED${NC}"
        PASSED_TESTS+=("$FILE_NAME download with timeout")
    fi
        sleep 0.2  # Pauza 0.2 sekundy mezi testy

done

echo -e "\nProcessing directory with options: $SOURCE_DIR_UPLOAD"
for LOCAL_FILE in $SOURCE_DIR_UPLOAD/*; do
    FILE_NAME=$(basename $LOCAL_FILE)
    REMOTE_FILE="$DEST_DIR_UPLOAD/$FILE_NAME"
    echo -n "Testing upload of $FILE_NAME with blksize option: "
    echo "Running: echo $REMOTE_FILE | ./tftp-client -h $SERVER -p $PORT -f $LOCAL_FILE -t $REMOTE_FILE --option blksize=8000"
    echo $REMOTE_FILE | ./tftp-client -h $SERVER -p $PORT -f $LOCAL_FILE -t $REMOTE_FILE --option blksize=8000
    if [[ $? -ne 0 ]]; then
        echo -e "${RED}FAILED${NC}"
        FAILED_TESTS+=("$FILE_NAME upload with blksize")
    else
        echo -e "${GREEN}PASSED${NC}"
        PASSED_TESTS+=("$FILE_NAME upload with blksize")
    fi

    echo -n "Testing upload of $FILE_NAME with timeout option: "
    echo "Running: echo $REMOTE_FILE | ./tftp-client -h $SERVER -p $PORT -f $LOCAL_FILE -t $REMOTE_FILE --option timeout=5"
    echo $REMOTE_FILE | ./tftp-client -h $SERVER -p $PORT -f $LOCAL_FILE -t $REMOTE_FILE --option timeout=5
    if [[ $? -ne 0 ]]; then
        echo -e "${RED}FAILED${NC}"
        FAILED_TESTS+=("$FILE_NAME upload with timeout")
    else
        echo -e "${GREEN}PASSED${NC}"
        PASSED_TESTS+=("$FILE_NAME upload with timeout")
    fi
done



# Compare the contents of the download directories
echo -e "\nComparing directories $SOURCE_DIR_DOWNLOAD and $DEST_DIR_DOWNLOAD"
diff -r $SOURCE_DIR_DOWNLOAD $DEST_DIR_DOWNLOAD
DIFF_STATUS=$?
if [ $DIFF_STATUS -ne 0 ]; then
    echo -e "${RED}The contents of $SOURCE_DIR_DOWNLOAD and $DEST_DIR_DOWNLOAD differ!${NC}"
    FAILED_TESTS+=("Download directory comparison")
else
    echo -e "${GREEN}The contents of $SOURCE_DIR_DOWNLOAD and $DEST_DIR_DOWNLOAD are identical!${NC}"
    PASSED_TESTS+=("Download directory comparison")
fi

# Compare the contents of the upload directories
echo -e "\nComparing directories $SOURCE_DIR_UPLOAD and $DEST_DIR_UPLOAD"
diff -r $SOURCE_DIR_UPLOAD $DEST_DIR_UPLOAD
DIFF_STATUS=$?
if [ $DIFF_STATUS -ne 0 ]; then
    echo -e "${RED}The contents of $SOURCE_DIR_UPLOAD and $DEST_DIR_UPLOAD differ!${NC}"
    FAILED_TESTS+=("Upload directory comparison")
else
    echo -e "${GREEN}The contents of $SOURCE_DIR_UPLOAD and $DEST_DIR_UPLOAD are identical!${NC}"
    PASSED_TESTS+=("Upload directory comparison")
fi

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