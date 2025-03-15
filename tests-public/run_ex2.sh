# Executable path
if [ -z "$1" ]; then
    echo "Usage: $0 <executable>"
    exit 1
fi
executable=$1

test_dir="tests-public/jobs2"
results_dir="tests-public/results2"

# Run executable and check results
for job_folder in "$test_dir"/*/; do
    echo -e "\e[34mRunning executable: $executable $job_folder 1 2 \e[0m"
    if ! ./"$executable" "$job_folder" 1 2; then
        echo -e "\e[31mExecutable failed\e[0m"
        exit 1
    fi

    # Iterate over each .out file in the generated output directory
    for output_file in "${job_folder}"*.out; do
        filename=$(basename "$output_file" .out)
        result=$(echo "$job_folder" | awk -F'/' '{print $(NF-1)}')
        result_file="${results_dir}/${result}/${filename}.result"
        echo "${result_file}"

        # Check the result file
        if [[ -f "$result_file" ]]; then
            if diff "$output_file" "$result_file"; then
                echo -e "\e[32mTest passed for $filename in $job_folder\e[0m"
            else
                echo -e "\e[31mTest failed for $filename in $job_folder\e[0m"
            fi
        else
            echo -e "\e[33mResult file not found for $filename in $job_folder\e[0m"
        fi
    done

    for output_file in "${job_folder}"*.bck; do
        filename=$(basename "$output_file" .bck)
        result=$(echo "$job_folder" | awk -F'/' '{print $(NF-1)}')
        result_file="${results_dir}/${result}/${filename}.bck"
        echo "${result_file}"

        # Check the result file
        if [[ -f "$result_file" ]]; then
            if diff "$output_file" "$result_file"; then
                echo -e "\e[32mTest passed for $filename in $job_folder\e[0m"
            else
                echo -e "\e[31mTest failed for $filename in $job_folder\e[0m"
            fi
        else
            echo -e "\e[33mResult file not found for $filename in $job_folder\e[0m"
        fi
    done
done
