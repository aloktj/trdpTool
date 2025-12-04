file(GLOB_RECURSE TRDP_CORE_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(trdp-core STATIC ${TRDP_CORE_SOURCES})

target_sources(trdp-core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tinyxml2.cpp)

target_include_directories(trdp-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${TRDP_INCLUDE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

target_compile_definitions(trdp-core PUBLIC $<$<BOOL:${TRDP_LIB_PATH}>:TRDP_AVAILABLE>)

if(TRDP_LIB_PATH)
    target_link_libraries(trdp-core PUBLIC ${TRDP_LIB_PATH})
endif()

