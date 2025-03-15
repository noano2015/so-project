#!/bin/bash

# Get binary path from command line arguments
if [ -z "$1" ]; then
    echo "Usage: $0 <executable>"
    exit
fi
kvs_binary=$1

test_dir="tests-public/jobs"
results_dir="tests-public/results"

run_test() {
    local file=$1
    local filename
    filename=$(basename "$file" .job)
    local temp_dir
    temp_dir=$(mktemp -d)

    cp "$file" "$temp_dir"

    local cmd="$kvs_binary $temp_dir 1 1" #single threaded

    eval "./$cmd" &> /dev/null

    local output_file
    output_file="${temp_dir}/${filename}.out"

    local result_file
    result_file="${results_dir}/${filename}.result"

    if [ -f "$output_file" ]; then
        # Compare the output with the expected result
        if diff "$output_file" "$result_file"; then
            echo -e "\e[32mTest passed for $filename\e[0m"
        else
            echo -e "\e[31mFiles $test_dir/$filename.out and $result_file differ\e[0m"
            echo -e "\e[31mTest failed for $filename\e[0m"
        fi
    else
        echo -e "\e[31mOutput file $output_file not found\e[0m"
    fi

    cp "$temp_dir"/"$filename".out "$test_dir"/"$filename".out
    rm -rf "$temp_dir"
}

export -f run_test
export test_dir results_dir ems_binary ems_args

# Loop through each .job file in the tests directory and run tests in parallel
for file in "$test_dir"/*.job; do
    run_test "$file"
done

# Wait for all background processes to complete
wait
