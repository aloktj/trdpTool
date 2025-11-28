add_executable(trdp-sim main.cpp)

target_link_libraries(trdp-sim PRIVATE trdp-core)

target_include_directories(trdp-sim PRIVATE ${TRDP_INCLUDE_DIR})

install(TARGETS trdp-sim RUNTIME DESTINATION bin)

