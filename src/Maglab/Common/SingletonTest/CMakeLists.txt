add_library(SingletonTest
    SingletonTest.cc
    SingletonTest.h
)

target_link_libraries(SingletonTest
    PUBLIC
        qgc
)

target_include_directories(SingletonTest
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
