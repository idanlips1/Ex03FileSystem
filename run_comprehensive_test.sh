#!/bin/bash
set -e

echo "Compiling comprehensive test..."
gcc fs.c comprehensive_test.c -o comprehensive_test

echo "Running comprehensive test..."
./comprehensive_test

echo "Comprehensive test completed!" 