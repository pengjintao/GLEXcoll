SET(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/build/lib")
add_compile_options(-O3)
include_directories("${PROJECT_SOURCE_DIR}/include")
include_directories("/usr/local/glex/include")
link_directories("/usr/local/glex/lib/")
AUX_SOURCE_DIRECTORY(${PROJECT_SOURCE_DIR}/src GLEXCOLL_SRC)
add_library(glexcoll STATIC ${GLEXCOLL_SRC})  
target_link_libraries(glexcoll -lrt -lglex ${MPI_LIBRARIES}  -lpthread -lrt)