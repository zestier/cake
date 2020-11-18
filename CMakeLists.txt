cmake_minimum_required(VERSION 3.6)
project(_all)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Initialize the needed conan files. Allows us to link to the libraries.
if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo_multi.cmake)
    include(${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo_multi.cmake)
else()
    include(${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo.cmake)
endif()
conan_basic_setup(TARGETS)

# Get rid of warnings missing debugging symbols
if (MSVC)
	target_link_options(CONAN_PKG::sdl2 INTERFACE "/ignore:4099")
endif()

# Force high error level. This should simply be useful for preventing pulling in
# libraries with higher error levels later.
if (MSVC)
#    add_compile_options(/W3 /WX)
    add_definitions(-D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING -D_CRT_SECURE_NO_WARNINGS)
else()
#    add_compile_options(-W -Wall -Werror)
endif()

set(CAKE_SOURCE_FILES
	src/main.cpp
	src/utils/bgfx_utils.h
	src/utils/bgfx_utils.cpp
)
add_executable(cake ${CAKE_SOURCE_FILES})
target_link_libraries(cake ${CONAN_TARGETS})
target_compile_definitions(cake PRIVATE CAKE_PKG_ROOT="${CMAKE_SOURCE_DIR}")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CAKE_SOURCE_FILES})

if (MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT cake)
else()
	set_target_properties(cake PROPERTIES
		SUFFIX ".html"
		LINK_FLAGS "-s DEMANGLE_SUPPORT=1 --bind -s MIN_WEBGL_VERSION=2 -s MAX_WEBGL_VERSION=2 -s FULL_ES3=1 --emrun -s ALLOW_MEMORY_GROWTH=1 -s USE_SDL=2 --preload-file assets"
	)
endif()

install(TARGETS cake
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)
  
install(FILES
	${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/*.js
	${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/*.wasm
	DESTINATION bin
)