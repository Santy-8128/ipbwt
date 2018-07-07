cmake_minimum_required(VERSION 3.10)
project(pbwt C)

set(CMAKE_C_STANDARD 11)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

#add_custom_target(pbwt COMMAND make -C /Users/sayantand/Software/pbwt/ CLION_EXE_DIR=/Users/sayantand/Software/pbwt)


execute_process(COMMAND date OUTPUT_VARIABLE DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND whoami OUTPUT_VARIABLE USER OUTPUT_STRIP_TRAILING_WHITESPACE)

add_definitions(-DVERSION="${PROJECT_VERSION}" -DUSER="${USER}" -DDATE="${DATE}")

find_package(ZLIB REQUIRED)


add_executable(pbwt
        src/array.c
        src/array.h
        src/dict.c
        src/dict.h
        src/hash.c
        src/hash.h
        src/pbwt.h
        src/pbwtCore.c
        src/pbwtGeneticMap.c
        src/pbwtHtslib.c
        src/pbwtImpute.c
        src/pbwtIO.c
        src/pbwtLikelihood.c
        src/pbwtMain.c
        src/pbwtMatch.c
        src/pbwtMerge.c
        src/pbwtPaint.c
        src/pbwtSample.c
        src/utils.c
        src/utils.h
        src/version.h)

include_directories(/Users/sayantand/Software/htslib/)
target_link_libraries(pbwt -lz -lm -lbz2 -llzma /Users/sayantand/Software/htslib/libhts.a ${STATGEN_LIBRARY} ${ZLIB_LIBRARIES})

install(TARGETS pbwt RUNTIME DESTINATION bin)


#add_custom_target(pbwt ALL COMMAND mingw32-make WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
