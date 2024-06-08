# It's important to specify the minimum CMake version upfront required by
# CMakeLists.txt. This is so that a user can clearly understand the reason the 
# build will fail before the build actually occurs, instead of searching for the
# CMake function that was used that is causing the failure.
cmake_minimum_required(VERSION 3.5)

# Set the global package-wide C standard. This will be inherited by all
# targets specified in the project.
set(CMAKE_C_STANDARD 11)

# Set the project name and version number. This allows for a user of your
# library or tool to specify a particular version when they include it, as in 
#   find_package(CMakeDemo 1.0 REQUIRED)
project(AllegroGame VERSION 1.0)
set(AllegroGame_VERSION 1.0)

add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

# Function to prepend the subdirectory to source files in subdirectories
FUNCTION(PREPEND var )
   SET(listVar "")
   FOREACH(f ${${var}})
       LIST(APPEND listVar "${CMAKE_CURRENT_SOURCE_DIR}/${f}")
   ENDFOREACH(f)
   SET(${var} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)

# After a normal build, we can specify the location of various outputs of the
# build. We put executables and static libraries outside the build directory in
# bin/ and lib/, respectively.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")

# Include source code and headers. This calls the CMakeLists.txt in each
# subdirectory. These can define their own libraries, executables, etc. as targets, 
# but here we define all exportable targets in the root CMakeLists.txt.
add_subdirectory(src)
add_subdirectory(include)

# Add the library CMakeDemo as a target, with the contents of src/ and include/
# as dependencies.
add_executable(AllegroGame ${AllegroGame_SRC} ${AllegroGame_INC})

# These variables slightly modify the install location to allow for version
# specific installations.
set(AllegroGame_INCLUDE_DEST "include/AllegroGame-${AllegroGame_VERSION}")
set(AllegroGame_LIB_DEST "lib/AllegroGame-${AllegroGame_VERSION}")

add_compile_options(-gdwarf-2 -fno-inline -fno-omit-frame-pointer)


# generator expressions are needed for the include directories, since installing headers changes the include path
# Specify that CMakeDemo requires the files located in the include/ directory at
# compile time. This would normally look like 
#   target_include_directories(CMakeDemo PUBLIC include/)
# PUBLIC means that other libraries including CMakeDemo should also include the
# directory include/.
# However, there is a catch. If we are installing the project in
# CMAKE_INSTALL_PREFIX, we can't specify include/ in the build directory: we have 
# copied the contents of include to CMAKE_INSTALL_PREFIX/include and we would
# like  other projects to include this directory instead of include/. The follow
# CMake command handles this. $<BUILD_INTERFACE:...> and
# $<INSTALL_INTERFACE:...> are macros whose values change depending on if we are
# simply building the code or if we are installing it.
target_include_directories(AllegroGame PUBLIC
   # headers to include when building from source
   $<BUILD_INTERFACE:${AllegroGame_SOURCE_DIR}/include> 
   $<BUILD_INTERFACE:${AllegroGame_BINARY_DIR}/include>

   # headers to include when installing  (implicitly prefixes with ${CMAKE_INSTALL_PREFIX}).
   $<INSTALL_INTERFACE:include> 
   )

# Specify that CMakeDemo requires LAPACK to link properly. Ideally, LAPACK would
# specify LAPACK::LAPACK for linking so that we can avoid using the variables.
# However, each package is different and one must check the documentation to 
# see what variables are defined.

target_link_libraries(AllegroGame wsock32 ws2_32)

target_link_libraries(AllegroGame ${AllegroGame_SOURCE_DIR}/deps/allegro/lib/liballegro_monolith.dll.a)
target_link_libraries(AllegroGame ${AllegroGame_SOURCE_DIR}/deps/allegro/lib/liballegro.dll.a)

target_include_directories(AllegroGame PUBLIC ${AllegroGame_SOURCE_DIR}/deps/allegro/include)
