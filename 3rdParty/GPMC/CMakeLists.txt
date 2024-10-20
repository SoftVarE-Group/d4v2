cmake_minimum_required(VERSION 3.0)

project(GMPC VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(GMP REQUIRED)
find_package(arjun REQUIRED)
find_package(ZLIB REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)
include_directories(${GMP_INCLUDE_DIRS})
include_directories(${GMPXX_INCLUDE_DIRS})
include_directories(${arjun_INCLUDE_DIR})
include_directories(${ZLIB_INCLUDE_DIR})

add_subdirectory(flow-cutter-pace17)

add_library(gpmc
	core/ComponentCache.cc
	core/ComponentManager.cc
	core/Config.cc
	core/Counter.cc
	core/gpmc.cpp
	core/Instance.cc
	core/Solver.cc
	ddnnf/DecisionTree.cc
	utils/Options.cc
	utils/System.cc
	preprocessor/Preprocessor.cc
	preprocessor/TestSolver.cc
	preprocessor/lib_sharpsat_td/subsumer.cpp
	preprocessor/TreeDecomposition.cc
	preprocessor/IFlowCutter.cc
)

set_target_properties(gpmc PROPERTIES PUBLIC_HEADER include/gpmc.hpp)

target_link_libraries(gpmc GMP::GMP GMP::GMPXX arjun ZLIB::ZLIB flowcutter)

install(TARGETS gpmc)
