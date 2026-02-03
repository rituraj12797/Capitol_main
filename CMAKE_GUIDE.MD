1. make the project directory 'my_project'
2. follow the following folder structure 


MyProject/
  |
  |___ CMakeLists.txt         <-- The Master Controller
  |
  |___ core/                  <-- YOUR LIBRARY (The "Package")
  |      |___ include/
  |      |      |___ lf_queue.h
  |      |___ src/
  |             |___ some_helper.cpp
  |
  |____ app/                   <-- YOUR EXECUTABLE (The Consumer)
  |       |___ main.cpp
  |
  |___ build/

  
3. source directory contains the main C++ file

4. Write the CMakeLists.txt file with 
    4.1 => CMake version => cmake_minimum_required(VERSION 3.10)

    4.2 => project(<NAME> VERSION <VERSION> LANGUAGES <LANGUAGES>)     => project(hello VERSION 1.0 LANGUAGES CXX)

    4.3 => set language to be used => set(CMAKE_CXX_STANDARD 17)
    4.4 => set mandatory rules for standard language => set(CMAKE_CXX_STANDARD_REQUIRED true)

    4.5 => Find threading library => find_package(Threads REQUIRED)

    4.6 => add address to the main executable file => add_executable(hello source/main.cpp)

    4.7 => Link threading library => target_link_libraries(hello PRIVATE Threads::Threads)

5. Go to build folder and run 
    
    for windows 
    ```
    cmake -G "MinGW Makefiles" ..  
    ```
    this tell the machine to use MinGW as the build system which is cross platform.
    *Note: Ensure your C:\mingw64\bin folder is in your System PATH for this to work in a terminal.*

    for linux system
    ```
    cmake ..
    ```



6.  This reads all necessary information from the CmakeLists.txt and writes a make file for the project 

7. Then in the build directory only run 
    ```
    cmake --build .
    ```
    this tells cmake to now actually build the executible using the makefile it generated.

8. This last step creates the .exe file which could be executed as 

    for windows 
    ``` ./hello.exe
    ```

    for linux system 
    ```
    ./hello
    ```
    