cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 20)

add_executable(imgnow WIN32
    main.cpp
    window.cpp window.h
    app.cpp app.h
    image.cpp image.h
    config.cpp config.h
    tinyfiledialogs.cpp tinyfiledialogs.h
    stb_image.h
    )

target_include_directories(imgnow PRIVATE
    "${PROJECT_SOURCE_DIR}/SDL/include"
    "${PROJECT_SOURCE_DIR}/clip"
    )

target_link_libraries(imgnow SDL3 SDL3main clip)
