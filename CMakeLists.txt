# Minimum required CMake version
cmake_minimum_required(VERSION 3.10)

# Project name and version
project(ServerChat VERSION 1.0 LANGUAGES C)

# Set the C standard (e.g., C99)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

# Add the executable
add_executable(serverChat chatBroadcast.c)

add_executable(clientChat clientChat.c)

# Include necessary libraries
# On Linux, link the required libraries if necessary (none are strictly required here)
# If you use additional libraries like pthread, you can add it:
# target_link_libraries(server_chat_fork_ipc PRIVATE pthread)
