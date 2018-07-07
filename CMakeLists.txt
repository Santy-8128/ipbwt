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
        array.c
        array.h
        dict.c
        dict.h
        hash.c
        hash.h
        pbwt.h
        pbwtCore.c
        pbwtGeneticMap.c
        pbwtHtslib.c
        pbwtImpute.c
        pbwtIO.c
        pbwtLikelihood.c
        pbwtMain.c
        pbwtMatch.c
        pbwtMerge.c
        pbwtPaint.c
        pbwtSample.c
        utils.c
        utils.h
        version.h)

include_directories(/Users/sayantand/Software/htslib/)
target_link_libraries(pbwt /Users/sayantand/Software/htslib/ibhts.a ${STATGEN_LIBRARY} ${ZLIB_LIBRARIES})

install(TARGETS pbwt RUNTIME DESTINATION bin)


#add_custom_target(pbwt ALL COMMAND mingw32-make WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
