cmake_minimum_required(VERSION 3.30)

project(d4 VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Solver)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# FIXME: libmtkahypar currently can not be built statically (https://github.com/kahypar/mt-kahypar/pull/161)
option(BUILD_SHARED_LIBS "Build shared libraries." ON)

if(NOT BUILD_SHARED_LIBS)
    set(GMP_USE_STATIC_LIBS ON)
    set(Boost_USE_STATIC_LIBS ON)
    set(MtKaHyPar_USE_STATIC_LIBS ON)
    set(glucose_USE_STATIC_LIBS ON)
endif()

set(GPMC_USE_STATIC_LIBS ON)

find_package(GMP REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options)
find_package(MtKaHyPar REQUIRED)
find_package(arjun REQUIRED)
find_package(GPMC REQUIRED)
find_package(glucose REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/glucose-3.0)
include_directories(${GMP_INCLUDE_DIRS})
include_directories(${GMPXX_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${arjun_INCLUDE_DIR})
include_directories(${GMPC_INCLUDE_DIR})
include_directories(${glucose_INCLUDE_DIRS})

add_library(caching OBJECT
    src/caching/BucketAllocator.cpp
    src/caching/cnf/BucketInConstruction.cpp
    src/caching/DataInfo.cpp
)

add_library(config OBJECT
    src/config/Config.cpp
    src/config/ConfigConverter.cpp
)

add_library(heuristics OBJECT
    src/heuristics/cnf/PartitioningHeuristicBipartite.cpp
    src/heuristics/cnf/PartitioningHeuristicBipartiteDual.cpp
    src/heuristics/cnf/PartitioningHeuristicBipartitePrimal.cpp
    src/heuristics/cnf/PartitioningHeuristicStatic.cpp
    src/heuristics/cnf/PartitioningHeuristicStaticMulti.cpp
    src/heuristics/cnf/PartitioningHeuristicStaticNone.cpp
    src/heuristics/cnf/PartitioningHeuristicStaticSingle.cpp
    src/heuristics/cnf/PartitioningHeuristicStaticSingleDual.cpp
    src/heuristics/cnf/PartitioningHeuristicStaticSinglePrimal.cpp
    src/heuristics/cnf/PhaseSelectorDynamic.cpp
    src/heuristics/cnf/PhaseSelectorManager.cpp
    src/heuristics/cnf/PhaseSelectorNone.cpp
    src/heuristics/cnf/PhaseSelectorStatic.cpp
    src/heuristics/cnf/ProjBackupHeuristicComponents.cpp
    src/heuristics/cnf/ProjBackupHeuristicHypergraph.cpp
    src/heuristics/cnf/ScoringMethodDlcs.cpp
    src/heuristics/cnf/ScoringMethodJwts.cpp
    src/heuristics/cnf/ScoringMethodMom.cpp
    src/heuristics/cnf/ScoringMethodVsads.cpp
    src/heuristics/cnf/ScoringMethodVsids.cpp
    src/heuristics/PartitioningHeuristic.cpp
    src/heuristics/PartitioningHeuristicNone.cpp
    src/heuristics/PhaseHeuristic.cpp
    src/heuristics/PhaseHeuristicFalse.cpp
    src/heuristics/PhaseHeuristicOccurrence.cpp
    src/heuristics/PhaseHeuristicPolarity.cpp
    src/heuristics/PhaseHeuristicTrue.cpp
    src/heuristics/ProjBackupHeuristic.cpp
    src/heuristics/ScoringMethod.cpp
)

add_library(hyperGraph OBJECT
    src/hyperGraph/HyperEdge.cpp
    src/hyperGraph/HyperGraph.cpp
    src/hyperGraph/HyperGraphExtractorDual.cpp
    src/hyperGraph/HyperGraphExtractorDualProj.cpp
    src/hyperGraph/HyperGraphExtractorPrimal.cpp
)

add_library(methods OBJECT
    src/methods/MethodManager.cpp
    src/methods/QueryManager.cpp
)

add_library(partitioner OBJECT
    src/partitioner/PartitionerKahyparMT.cpp
    src/partitioner/PartitionerManager.cpp
)

add_library(preprocs OBJECT
    src/preprocs/cnf/util/lib_sharpsat_td/subsumer.cpp
    src/preprocs/cnf/util/PreprocGPMC.cpp
    src/preprocs/cnf/util/TestSolver.cpp
    src/preprocs/cnf/PreprocBackboneCnf.cpp
    src/preprocs/cnf/PreprocBasicCnf.cpp
    src/preprocs/cnf/PreprocGPMC.cpp
    src/preprocs/cnf/PreprocProj.cpp
    src/preprocs/PreprocManager.cpp
)

add_library(problem OBJECT
    src/problem/cnf/ParserDimacs.cpp
    src/problem/cnf/ProblemManagerCnf.cpp
    src/problem/ProblemManager.cpp
    src/problem/ProblemTypes.cpp
)

add_library(solvers OBJECT
    src/solvers/ActivityManager.cpp
    src/solvers/cnf/minisat/Solver.cpp
    src/solvers/cnf/WrapperMinisat.cpp
    src/solvers/WrapperSolver.cpp
)

if(USE_GLUCOSE)
    target_sources(solvers PRIVATE src/solvers/cnf/WrapperGlucose.cpp)
endif()

add_library(spec OBJECT
    src/specs/cnf/SpecManagerCnf.cpp
    src/specs/cnf/SpecManagerCnfDyn.cpp
    src/specs/SpecManager.cpp
)

add_library(utils OBJECT
    src/utils/AtMost1Extractor.cpp
    src/utils/cnf/AndGatesExtractor.cpp
    src/utils/EquivExtractor.cpp
)

add_library(core STATIC
    $<TARGET_OBJECTS:config>
    $<TARGET_OBJECTS:caching>
    $<TARGET_OBJECTS:heuristics>
    $<TARGET_OBJECTS:hyperGraph>
    $<TARGET_OBJECTS:methods>
    $<TARGET_OBJECTS:partitioner>
    $<TARGET_OBJECTS:preprocs>
    $<TARGET_OBJECTS:problem>
    $<TARGET_OBJECTS:solvers>
    $<TARGET_OBJECTS:spec>
    $<TARGET_OBJECTS:utils>
)

target_link_libraries(core MtKaHyPar::mtkahypar GPMC::GPMC arjun glucose::glucose GMP::GMPXX GMP::GMP cadiback cadical)

if(USE_GLUCOSE)
    target_link_libraries(core glucose::glucose)
endif()

add_executable(d4 src/Main.cpp)
target_link_libraries(d4 core Boost::program_options)

install(TARGETS d4)
