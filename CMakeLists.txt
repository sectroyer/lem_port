cmake_minimum_required(VERSION 3.20)

# iOS builds target iOS 12 so older devices (e.g. iPad mini 2) are supported.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0 CACHE STRING "Minimum iOS version")
endif()

project(lem_port VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(LEM_IOS ON)
else()
  set(LEM_IOS OFF)
endif()

option(LEM_BUILD_APP "Build the SDL3 game frontend" ON)
option(LEM_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(LEM_FETCH_SDL "Download and build SDL3 instead of using an installed copy" ${LEM_IOS})
if(LEM_IOS)
  set(LEM_BUILD_TESTS OFF)
endif()

set(LEM_BUNDLE_ID "io.github.sectroyer.lemport" CACHE STRING "Application bundle identifier")

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# Platform independent engine: file formats, simulation, rendering to indexed
# framebuffers. No SDL dependency so it can be reused by every frontend.
add_library(lemcore STATIC
  src/core/dat.cpp
  src/core/bitmap.cpp
  src/core/level.cpp
  src/core/graphicset.cpp
  src/core/levelrender.cpp
  src/core/maindat.cpp
  src/core/levelpack.cpp
  src/core/game.cpp
  src/core/password.cpp
  src/core/x86.cpp
  src/core/adlib.cpp
  src/core/fileutil.cpp
)
target_include_directories(lemcore PUBLIC src/core)

# Third-party: ymfm (BSD-3-Clause) for the OPL2 FM chip.
add_library(ymfm STATIC
  third_party/ymfm/ymfm_opl.cpp
  third_party/ymfm/ymfm_adpcm.cpp
  third_party/ymfm/ymfm_pcm.cpp
)
target_include_directories(ymfm PUBLIC third_party/ymfm)
target_compile_options(ymfm PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>)
target_link_libraries(lemcore PUBLIC ymfm)

if(NOT LEM_IOS)
  add_executable(lemtool src/tools/lemtool.cpp src/tools/png.cpp)
  target_link_libraries(lemtool PRIVATE lemcore)
endif()

if(LEM_BUILD_APP)
  if(LEM_FETCH_SDL)
    include(FetchContent)
    FetchContent_Declare(SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-3.4.20
      GIT_SHALLOW TRUE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3)
    set(LEM_SDL_TARGET SDL3::SDL3-static)
  else()
    find_package(SDL3 REQUIRED CONFIG)
    set(LEM_SDL_TARGET SDL3::SDL3)
  endif()

  add_executable(lem_port WIN32 MACOSX_BUNDLE
    src/app/main.cpp
    src/app/config.cpp
    src/app/screens.cpp
    src/app/audio.cpp
  )
  target_link_libraries(lem_port PRIVATE lemcore ${LEM_SDL_TARGET})
  set_target_properties(lem_port PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME lem_port
    MACOSX_BUNDLE_GUI_IDENTIFIER ${LEM_BUNDLE_ID}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})

  if(LEM_IOS)
    # Icons and launch screen; the icon is original artwork (tools/make_icon.py).
    set(LEM_IOS_RES
      res/ios/LaunchScreen.storyboard
      res/ios/AppIcon60x60@2x.png
      res/ios/AppIcon60x60@3x.png
      res/ios/AppIcon76x76.png
      res/ios/AppIcon76x76@2x.png
      res/ios/AppIcon83.5x83.5@2x.png)
    target_sources(lem_port PRIVATE ${LEM_IOS_RES})
    set_source_files_properties(${LEM_IOS_RES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(lem_port PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/res/ios/Info.plist.in
      RESOURCE "${LEM_IOS_RES}"
      XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER ${LEM_BUNDLE_ID}
      XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET ${CMAKE_OSX_DEPLOYMENT_TARGET}
      XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
      XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
      XCODE_ATTRIBUTE_COMBINE_HIDPI_IMAGES NO)
  elseif(APPLE)
    target_sources(lem_port PRIVATE res/lem_port.icns)
    set_source_files_properties(res/lem_port.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(lem_port PROPERTIES MACOSX_BUNDLE_ICON_FILE lem_port.icns)
  endif()
endif()

if(LEM_BUILD_TESTS)
  enable_testing()
  add_executable(lemtests tests/test_main.cpp)
  target_link_libraries(lemtests PRIVATE lemcore)
  add_test(NAME lemtests COMMAND lemtests)

  # Regression tests with the original data (skipped unless LEM_DATA is set).
  add_executable(data_tests tests/data_tests.cpp)
  target_link_libraries(data_tests PRIVATE lemcore)
  add_test(NAME data_tests COMMAND data_tests ${CMAKE_SOURCE_DIR}/tests/replays
                                              ${CMAKE_SOURCE_DIR}/tests/golden.txt)
  set_tests_properties(data_tests PROPERTIES SKIP_RETURN_CODE 77)
endif()
