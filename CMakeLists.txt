cmake_minimum_required(VERSION 3.21)
project(Menhir VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# ── ASTERIXCodec dependency ───────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    ASTERIXCodec
    GIT_REPOSITORY https://github.com/nathan-casabieille/ASTERIXCodec.git
    GIT_TAG        main
)
set(ASTERIX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ASTERIXCodec)

# ── App ───────────────────────────────────────────────────────────────────────
set(APP_ICON_MACOS "${CMAKE_CURRENT_SOURCE_DIR}/resources/menhir.icns")
set(APP_ICON_WIN   "${CMAKE_CURRENT_SOURCE_DIR}/resources/menhir.ico")

add_executable(Menhir
    src/main.cpp
    src/MainWindow.cpp
    src/HexEditor.cpp
    src/DecodeView.cpp
    src/EncodePanel.cpp
    src/ByteTracker.cpp
    resources/specs.qrc
    $<$<BOOL:${APPLE}>:${APP_ICON_MACOS}>
    $<$<BOOL:${WIN32}>:${APP_ICON_WIN}>
)

target_link_libraries(Menhir PRIVATE Qt6::Widgets ASTERIXCodec)
target_include_directories(Menhir PRIVATE src)

if(APPLE)
    set_target_properties(Menhir PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "fr.oratis.menhir"
        MACOSX_BUNDLE_BUNDLE_VERSION     ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
        MACOSX_BUNDLE_ICON_FILE          "menhir.icns"
    )
    set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
endif()

if(WIN32)
    set_target_properties(Menhir PROPERTIES
        WIN32_EXECUTABLE TRUE
        RC_ICONS ${APP_ICON_WIN}
    )
endif()
