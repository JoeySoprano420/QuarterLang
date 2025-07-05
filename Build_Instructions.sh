g++ main.cpp QuarterLang_Compiler_Pipeline.cpp -o qtr_demo -std=c++17
./qtr_demo

mkdir build && cd build
cmake ..
make
./qtr_demo
