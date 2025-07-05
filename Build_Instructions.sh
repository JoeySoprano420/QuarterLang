#!/bin/bash

# ============================
# QuarterLang Build Script 🛠️
# ============================

# Step 1: Compile the full compiler pipeline
echo "🔧 Compiling QuarterLang compiler..."
g++ main.cpp QuarterLang_Compiler_Pipeline.cpp -o qtr_demo -std=c++17 -Wall -Wextra -O2

# Step 2: Check for successful compilation
if [ $? -ne 0 ]; then
    echo "❌ Compilation failed. Please check for errors."
    exit 1
fi

# Step 3: Run unit tests
echo "🧪 Running unit tests..."
./qtr_demo test
if [ $? -ne 0 ]; then
    echo "❌ Unit tests failed."
    exit 1
fi

# Step 4: Run the REPL (optional interactive mode)
echo "💬 Launching QuarterLang REPL..."
./qtr_demo repl


g++ main.cpp QuarterLang_Compiler_Pipeline.cpp -o qtr_demo -std=c++17
./qtr_demo

mkdir build && cd build
cmake ..
make
./qtr_demo
