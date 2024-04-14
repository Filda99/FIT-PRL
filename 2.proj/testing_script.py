"""
File: testing_script.cpp (for Game of Life)
Author: Michal Luner (xluner01)
Subject: PRL
Date: 12.04.2024
"""

import os
import subprocess
import re
from tqdm import tqdm

PASS_COLOUR = '\033[92m'
FAIL_COLOUR = '\033[91m'
DEFAULT_COLOUR = '\033[0m'

# Compile the program
compile_command = "mpic++ --prefix /usr/local/share/OpenMPI -std=c++17 -o life life.cpp"
subprocess.run(compile_command, shell=True, check=True)

# Define the test directories
test_directories = ["my_tests/{}x{}".format(size, size) for size in range(4, 12)]

# Function to run the program with a given input file and number of steps
def run_program(input_file, num_steps, num_lines):
    command = f"mpirun --oversubscribe --prefix /usr/local/share/OpenMPI -np {num_lines} life {input_file} {num_steps}"
    result = subprocess.run(command, shell=True, capture_output=True, text=True)
    output = result.stdout.strip()
    # Split the output into lines and strip the leading index and colon
    lines = output.split('\n')
    output_without_index = '\n'.join(line.split(': ')[1] for line in lines)
    return output_without_index

# Function to compare the program output with the expected output
def compare_outputs(output, expected_output_file):
    with open(expected_output_file) as f:
        expected_output = f.read().strip()
    # print(f"Output:\n{output}")
    # print(f"Expected output:\n{expected_output}")
    return output == expected_output

# Iterate over test directories
for test_dir in tqdm(test_directories):
    test_cases = os.listdir(test_dir)
    for case in tqdm(test_cases, desc=f"Running tests in {test_dir}"):
        case_dir = os.path.join(test_dir, case)
        if not os.path.isdir(case_dir):
            continue
        
        input_file = os.path.join(case_dir, "input.txt")
        # Find all files in the case directory with the format i_number.txt
        output_files = [f for f in os.listdir(case_dir) if re.match(r"i_\d+\.txt", f)]
        for output_file in output_files:
            # Extract the number of steps from the filename
            match = re.match(r"i_(\d+)\.txt", output_file)
            if match:
                num_steps = int(match.group(1))
            else:
                print(f"Invalid filename format: {input_file}")
                continue
            
            # number of lines is always equal to the name of the test directory
            num_lines = int(test_dir.split('/')[-1].split('x')[0])
            
            # Run the program
            program_output = run_program(input_file, num_steps, num_lines)
            
            # Compare output with expected output
            output_file_path = os.path.join(test_dir, case, output_file)

            if compare_outputs(program_output, output_file_path):
                print(f"{PASS_COLOUR}PASSED{DEFAULT_COLOUR} for {output_file} \t in {case_dir}")
            else:
                print(f"{FAIL_COLOUR}FAILED{DEFAULT_COLOUR} for {output_file} \t in {case_dir}")