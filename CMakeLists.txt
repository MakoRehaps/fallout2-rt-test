cmake_minimum_required(VERSION 3.13)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(EXECUTABLE_NAME fallout2-ce)

if(APPLE)
    if(IOS)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11" CACHE STRING "")
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
    else()
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.11" CACHE STRING "")
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
    endif()
endif()

project(${EXECUTABLE_NAME})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

if(ANDROID)
    add_library(${EXECUTABLE_NAME} SHARED)
else()
    add_executable(${EXECUTABLE_NAME} WIN32 MACOSX_BUNDLE)
endif()

# Group source files into folders
file(GLOB_RECURSE SOURCE_FILES
    "src/*.cc"
    "src/*.h"
    "src/platform/*.h"
    "src/platform/*.mm"
    "os/windows/*.ico"
    "os/windows/*.rc"
    "os/ios/*.xcassets"
    "os/ios/*.storyboard"
    "os/macos/*.icns"
)

# Add source files to the target
target_sources(${EXECUTABLE_NAME} PUBLIC ${SOURCE_FILES})

if(IOS)
    set(RESOURCES
        "os/ios/AppIcon.xcassets"
        "os/ios/LaunchScreen.storyboard"
    )

    target_sources(${EXECUTABLE_NAME} PUBLIC ${RESOURCES})
    set_source_files_properties(${RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/os/ios/Info.plist"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.alexbatalov.fallout2-ce"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    )

    set(MACOSX_BUNDLE_BUNDLE_NAME "${EXECUTABLE_NAME}")
    set(MACOSX_BUNDLE_DISPLAY_NAME "Fallout 2")
else()
    set(RESOURCES
        "os/macos/fallout2-ce.icns"
    )

    target_sources(${EXECUTABLE_NAME} PUBLIC ${RESOURCES})
    set_source_files_properties(${RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        OUTPUT_NAME "Fallout II Community Edition"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/os/macos/Info.plist"
        XCODE_ATTRIBUTE_EXECUTABLE_NAME "${EXECUTABLE_NAME}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.alexbatalov.fallout2-ce"
    )

    set(MACOSX_BUNDLE_ICON_FILE "fallout2-ce.icns")
    set(MACOSX_BUNDLE_BUNDLE_NAME "Fallout II: Community Edition")
    set(MACOSX_BUNDLE_DISPLAY_NAME "Fallout II")
endif()

set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.alexbatalov.fallout2-ce")
set(MACOSX_BUNDLE_SHORT_VERSION_STRING "1.2.0")
set(MACOSX_BUNDLE_BUNDLE_VERSION "1.2.0")

if(WIN32)
    target_compile_definitions(${EXECUTABLE_NAME} PUBLIC
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )

    target_link_libraries(${EXECUTABLE_NAME}
        winmm
    )

    target_sources(${EXECUTABLE_NAME} PUBLIC
        "os/windows/fallout2-ce.ico"
        "os/windows/fallout2-ce.rc"
    )
endif()

add_subdirectory("third_party/fpattern")
target_link_libraries(${EXECUTABLE_NAME} ${FPATTERN_LIBRARY})
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${FPATTERN_INCLUDE_DIR})

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_subdirectory("third_party/zlib")
    add_subdirectory("third_party/sdl2")
else()
    find_package(ZLIB)
    find_package(SDL2)
endif()

target_link_libraries(${EXECUTABLE_NAME} ${ZLIB_LIBRARIES})
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${ZLIB_INCLUDE_DIRS})

target_link_libraries(${EXECUTABLE_NAME} ${SDL2_LIBRARIES})
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})

if(APPLE)
    if(IOS)
        install(TARGETS ${EXECUTABLE_NAME} DESTINATION "Payload")

        set(CPACK_GENERATOR "ZIP")
        set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
        set(CPACK_PACKAGE_FILE_NAME "fallout2-ce")
        set(CPACK_ARCHIVE_FILE_EXTENSION "ipa")
    else()
        install(TARGETS ${EXECUTABLE_NAME} DESTINATION .)

        if(CPACK_BUNDLE_APPLE_CERT_APP)
            install(CODE "
                execute_process(COMMAND codesign --deep --force --options runtime --sign \"${CPACK_BUNDLE_APPLE_CERT_APP}\" ${CMAKE_BINARY_DIR}/Fallout II Community Edition.app)
                "
                COMPONENT Runtime)
        endif()

        set(CPACK_GENERATOR "DragNDrop")
        set(CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK ON)
        set(CPACK_PACKAGE_FILE_NAME "Fallout II Community Edition")
    endif()

    include(CPack)
endif()
