# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/xiurui/sponge

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/xiurui/sponge/bulid

# Utility rule file for check_lab0.

# Include the progress variables for this target.
include CMakeFiles/check_lab0.dir/progress.make

CMakeFiles/check_lab0:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/xiurui/sponge/bulid/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Testing Lab 0..."
	/usr/bin/ctest --output-on-failure --timeout 10 -R 't_webget|t_byte_stream|_dt'

check_lab0: CMakeFiles/check_lab0
check_lab0: CMakeFiles/check_lab0.dir/build.make

.PHONY : check_lab0

# Rule to build all files generated by this target.
CMakeFiles/check_lab0.dir/build: check_lab0

.PHONY : CMakeFiles/check_lab0.dir/build

CMakeFiles/check_lab0.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/check_lab0.dir/cmake_clean.cmake
.PHONY : CMakeFiles/check_lab0.dir/clean

CMakeFiles/check_lab0.dir/depend:
	cd /home/xiurui/sponge/bulid && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/xiurui/sponge /home/xiurui/sponge /home/xiurui/sponge/bulid /home/xiurui/sponge/bulid /home/xiurui/sponge/bulid/CMakeFiles/check_lab0.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/check_lab0.dir/depend

