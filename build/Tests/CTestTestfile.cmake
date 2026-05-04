# CMake generated Testfile for 
# Source directory: C:/Users/Antoan/Downloads/LoopFinder/Tests
# Build directory: C:/Users/Antoan/Downloads/LoopFinder/build/Tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test([=[LoopDetectorTests]=] "C:/Users/Antoan/Downloads/LoopFinder/build/Tests/LoopDetectorTests_artefacts/Debug/LoopDetectorTests.exe")
  set_tests_properties([=[LoopDetectorTests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/Antoan/Downloads/LoopFinder/Tests/CMakeLists.txt;26;add_test;C:/Users/Antoan/Downloads/LoopFinder/Tests/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test([=[LoopDetectorTests]=] "C:/Users/Antoan/Downloads/LoopFinder/build/Tests/LoopDetectorTests_artefacts/Release/LoopDetectorTests.exe")
  set_tests_properties([=[LoopDetectorTests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/Antoan/Downloads/LoopFinder/Tests/CMakeLists.txt;26;add_test;C:/Users/Antoan/Downloads/LoopFinder/Tests/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test([=[LoopDetectorTests]=] "C:/Users/Antoan/Downloads/LoopFinder/build/Tests/LoopDetectorTests_artefacts/MinSizeRel/LoopDetectorTests.exe")
  set_tests_properties([=[LoopDetectorTests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/Antoan/Downloads/LoopFinder/Tests/CMakeLists.txt;26;add_test;C:/Users/Antoan/Downloads/LoopFinder/Tests/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test([=[LoopDetectorTests]=] "C:/Users/Antoan/Downloads/LoopFinder/build/Tests/LoopDetectorTests_artefacts/RelWithDebInfo/LoopDetectorTests.exe")
  set_tests_properties([=[LoopDetectorTests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/Antoan/Downloads/LoopFinder/Tests/CMakeLists.txt;26;add_test;C:/Users/Antoan/Downloads/LoopFinder/Tests/CMakeLists.txt;0;")
else()
  add_test([=[LoopDetectorTests]=] NOT_AVAILABLE)
endif()
