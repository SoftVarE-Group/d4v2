SET(flowcutter_SOURCES 
	src/cell.cpp 
	src/greedy_order.cpp
	src/list_graph.cpp
	src/tree_decomposition.cpp)

add_compile_options("$<$<CONFIG:Debug>:-O0>")

add_library(flowcutter ${flowcutter_SOURCES})

target_compile_definitions(flowcutter 
  PRIVATE $<$<CONFIG:Debug>:DEBUG>)