#!/bin/bash
set -e

echo "Compiling stress tests..."
gcc fs.c test_stress.c -o test_stress

echo "Running stress tests..."
./test_stress

echo "Stress tests completed!" 