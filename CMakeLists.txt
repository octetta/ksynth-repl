cmake_minimum_required(VERSION 3.14)
project(ksynth-repl CXX C)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure hazel
add_subdirectory(third_party/hazel)

# Set path to k-synth source
set(KSYNTH_DIR "/home/stewartj/book/k-synth")

add_executable(ksynth-repl MACOSX_BUNDLE WIN32 src/main.cpp ${KSYNTH_DIR}/ksynth.c ${KSYNTH_DIR}/ks_api.c ${KSYNTH_DIR}/miniaudio.c ${KSYNTH_DIR}/kgnuplot.c)
target_include_directories(ksynth-repl PRIVATE ${KSYNTH_DIR})

if(WIN32)
    target_link_libraries(ksynth-repl PRIVATE hazel ws2_32 winmm ole32)
elseif(APPLE)
    target_link_libraries(ksynth-repl PRIVATE hazel "-framework CoreFoundation" "-framework CoreMIDI" "-framework CoreAudio")
else()
    target_link_libraries(ksynth-repl PRIVATE hazel asound m)
endif()
