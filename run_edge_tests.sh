#!/bin/bash
set -e

echo "Compiling edge case tests..."
gcc fs.c test_edge_cases.c -o test_edge_cases

echo "Running edge case tests..."
./test_edge_cases

echo "Edge case tests completed!" 