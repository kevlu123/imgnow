cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 20)

add_executable(imgnow WIN32
    main.cpp
    window.cpp window.h
    app.cpp app.h
    image.cpp image.h
    config.cpp config.h
    colourfmt.cpp colourfmt.h
    tinyfiledialogs.cpp tinyfiledialogs.h
    stb_image.h
    "${PROJECT_SOURCE_DIR}/icon/icon.cpp" "${PROJECT_SOURCE_DIR}/icon/icon.h"
    "${PROJECT_SOURCE_DIR}/icon/icon.rc"
    )
	
target_include_directories(imgnow PRIVATE
    "${PROJECT_SOURCE_DIR}/icon"
    "${PROJECT_SOURCE_DIR}/SDL/include"
    "${PROJECT_SOURCE_DIR}/clip"
    )

target_link_libraries(imgnow SDL3-static SDL3main clip)
