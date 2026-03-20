cmake_minimum_required(VERSION 3.5)

if(DEFINED TIVARS_ENABLE_FUZZING)
    set(TIVARS_ENABLE_FUZZING "${TIVARS_ENABLE_FUZZING}" CACHE BOOL "Build libFuzzer targets" FORCE)
endif()

if(APPLE AND DEFINED TIVARS_ENABLE_FUZZING AND TIVARS_ENABLE_FUZZING)
    set(TIVARS_HOMEBREW_LLVM_PREFIXES
            /opt/homebrew/opt/llvm
            /usr/local/opt/llvm
    )

    foreach(TIVARS_LLVM_PREFIX IN LISTS TIVARS_HOMEBREW_LLVM_PREFIXES)
        if(EXISTS "${TIVARS_LLVM_PREFIX}/bin/clang" AND EXISTS "${TIVARS_LLVM_PREFIX}/bin/clang++")
            set(CMAKE_C_COMPILER "${TIVARS_LLVM_PREFIX}/bin/clang" CACHE FILEPATH "C compiler" FORCE)
            set(CMAKE_CXX_COMPILER "${TIVARS_LLVM_PREFIX}/bin/clang++" CACHE FILEPATH "C++ compiler" FORCE)
            message(STATUS "Using Homebrew LLVM for fuzzing: ${TIVARS_LLVM_PREFIX}")
            break()
        endif()
    endforeach()
endif()

project(tivars_lib_cpp
        VERSION     1.3.0
        LANGUAGES   CXX)

include(CheckCXXCompilerFlag)
if(MSVC)
    CHECK_CXX_COMPILER_FLAG("/std:c++20" COMPILER_SUPPORTS_CXX20)
else()
    CHECK_CXX_COMPILER_FLAG("-std=c++2a" COMPILER_SUPPORTS_CXX20)
endif()
if(COMPILER_SUPPORTS_CXX20)
    set(CMAKE_CXX_STANDARD 20)
    add_definitions(-DTH_GDB_SUPPORT=1)
else()
    set(CMAKE_CXX_STANDARD 11)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TIVARS_ENABLE_FUZZING "Build libFuzzer targets" OFF)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 ")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -Wextra -Werror=write-strings -Werror=redundant-decls -Werror=format -Werror=format-security -Werror=date-time -Werror=return-type -Werror=pointer-arith -Winit-self -Wno-unused-parameter ")
endif()

file(GLOB HEADER_FILES "src/*.h" "src/TypeHandlers/*.h")
file(GLOB COMMON_SOURCE ${HEADER_FILES} "src/*.cpp" "src/TypeHandlers/*.cpp")

if(NOT WIN32)
    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address,undefined")
    set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address,undefined")
endif()

if(TIVARS_ENABLE_FUZZING)
    if(MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "TIVARS_ENABLE_FUZZING requires Clang (with libFuzzer support)")
    endif()
endif()

add_executable(tivars_lib_cpp_tests ${COMMON_SOURCE} tests.cpp)
add_executable(tivars_lib_cpp_cli   ${COMMON_SOURCE} cli/cli.cpp)

if(TIVARS_ENABLE_FUZZING)
    set(TIVARS_FUZZ_CORPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus)

    add_custom_target(tivars_fuzzer_seed_corpus
            COMMAND ${CMAKE_COMMAND} -E make_directory ${TIVARS_FUZZ_CORPUS_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/testData ${TIVARS_FUZZ_CORPUS_DIR}
            COMMENT "Seeding libFuzzer corpus from testData"
    )

    add_executable(tivars_lib_cpp_fuzzer ${COMMON_SOURCE} fuzz/tivars_file_fuzzer.cpp)
    add_dependencies(tivars_lib_cpp_fuzzer tivars_fuzzer_seed_corpus)
    target_compile_definitions(tivars_lib_cpp_fuzzer PRIVATE
            FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION=1
            TIVARS_PROGRAMS_TOKENS_CSV="${CMAKE_CURRENT_SOURCE_DIR}/programs_tokens.csv"
    )
    target_compile_options(tivars_lib_cpp_fuzzer PRIVATE
            -O1
            -g
            -fno-omit-frame-pointer
            -fsanitize=fuzzer-no-link,address,undefined
    )
    set_property(TARGET tivars_lib_cpp_fuzzer APPEND_STRING PROPERTY LINK_FLAGS " -fno-omit-frame-pointer -fsanitize=fuzzer,address,undefined")
endif()

if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)

    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS deployment version" FORCE)
    endif()

    set(CMAKE_OBJCXX_STANDARD ${CMAKE_CXX_STANDARD})
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

    find_program(CODESIGN_EXECUTABLE codesign)

    set(TIVARS_QUICKLOOK_APP_BUNDLE_IDENTIFIER "com.adriweb.tivars-lib-cpp.quicklook")
    set(TIVARS_QUICKLOOK_PREVIEW_BUNDLE_IDENTIFIER "${TIVARS_QUICKLOOK_APP_BUNDLE_IDENTIFIER}.preview")
    set(TIVARS_QUICKLOOK_THUMBNAIL_BUNDLE_IDENTIFIER "${TIVARS_QUICKLOOK_APP_BUNDLE_IDENTIFIER}.thumbnail")
    set(TIVARS_QUICKLOOK_UTI "com.adriweb.tivars.calculator-file")

    if (DEFINED ENV{CI})
        set(TIVARS_QUICKLOOK_CODESIGN_IDENTITY "-" CACHE STRING "Code signing identity")
    else()
        set(TIVARS_QUICKLOOK_CODESIGN_IDENTITY "Developer ID Application: Adrien Bertrand (Z3B7V95FNU)" CACHE STRING "Code signing identity")
    endif()

    set(TIVARS_QUICKLOOK_EXTENSIONS
            82n 83n 8xn
            82l 83l 8xl
            82m 83m 8xm
            82y 83y 8xy
            82s 83s 8xs
            82p 83p 8xp
            82i 83i 8xi 8ci
            82d 83d 8xd
            83c 8xc
            82w 83w 8xw
            82z 83z 8xz
            82t 83t 8xt
            82b 83b 8xb 8cb
            8xv
            82g 83g 8xg 8cg
            8ca
            82u 8xu 8cu 8eu 8pu 8yu
            8xk 8ck 8ek
            8xq 8cq
    )

    set(TIVARS_QUICKLOOK_EXTENSION_XML "")
    set(TIVARS_QUICKLOOK_IMPORTED_ALIAS_UTI_XML "")
    set(TIVARS_QUICKLOOK_SUPPORTED_CONTENT_TYPE_XML "                <string>${TIVARS_QUICKLOOK_UTI}</string>\n")
    foreach(ext IN LISTS TIVARS_QUICKLOOK_EXTENSIONS)
        string(APPEND TIVARS_QUICKLOOK_EXTENSION_XML "                <string>${ext}</string>\n")
        if(ext STREQUAL "8xp")
            set(EXTRA_UTTypeConformsTo "                <string>org.revsoft.wabbitemu.program</string>\n")
        elseif(exp STREQUAL "8xg")
            set(EXTRA_UTTypeConformsTo "                <string>org.revsoft.wabbitemu.group</string>\n")
        elseif(exp STREQUAL "8xi")
            set(EXTRA_UTTypeConformsTo "                <string>org.revsoft.wabbitemu.picture</string>\n")
        elseif(exp STREQUAL "8xk")
            set(EXTRA_UTTypeConformsTo "                <string>org.revsoft.wabbitemu.application</string>\n")
        else()
            set(EXTRA_UTTypeConformsTo "")
        endif()
        string(APPEND TIVARS_QUICKLOOK_IMPORTED_ALIAS_UTI_XML
"        <dict>\n\
            <key>UTTypeIdentifier</key>\n\
            <string>com.ti.et.core.datatypes.${ext}</string>\n\
            <key>UTTypeDescription</key>\n\
            <string>TI calculator file (${ext})</string>\n\
            <key>UTTypeConformsTo</key>\n\
            <array>\n\
                <string>${TIVARS_QUICKLOOK_UTI}</string>\n\
                <string>com.ti.et.core.datatypes.${ext}</string>\n${EXTRA_UTTypeConformsTo}\
            </array>\n\
            <key>UTTypeTagSpecification</key>\n\
            <dict>\n\
                <key>public.filename-extension</key>\n\
                <array>\n\
                    <string>${ext}</string>\n\
                </array>\n\
            </dict>\n\
        </dict>\n")
    endforeach()

    configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/quicklook/App-Info.plist.in
            ${CMAKE_CURRENT_BINARY_DIR}/quicklook/App-Info.plist
            @ONLY
    )
    configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/quicklook/Preview-Info.plist.in
            ${CMAKE_CURRENT_BINARY_DIR}/quicklook/Preview-Info.plist
            @ONLY
    )
    configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/quicklook/Thumbnail-Info.plist.in
            ${CMAKE_CURRENT_BINARY_DIR}/quicklook/Thumbnail-Info.plist
            @ONLY
    )

    set(TIVARS_QUICKLOOK_OBJC_SOURCES
            quicklook/TIVarsQuickLookSupport.mm
            quicklook/PreviewProvider.mm
            quicklook/ThumbnailProvider.mm
            quicklook/AppDelegate.m
            quicklook/main.m
    )
    set_source_files_properties(${TIVARS_QUICKLOOK_OBJC_SOURCES} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")

    add_library(tivars_quicklook_support OBJECT
            ${COMMON_SOURCE}
            quicklook/TIVarsQuickLookSupport.mm
    )
    set_target_properties(tivars_quicklook_support PROPERTIES POSITION_INDEPENDENT_CODE ON)

    add_executable(tivars_quicklook_preview_extension MACOSX_BUNDLE
            $<TARGET_OBJECTS:tivars_quicklook_support>
            quicklook/PreviewProvider.mm
            quicklook/ExtensionMain.m
            programs_tokens.csv
    )
    set_source_files_properties(programs_tokens.csv PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(tivars_quicklook_preview_extension PROPERTIES
            BUNDLE_EXTENSION appex
            OUTPUT_NAME TIVarsQuickLookPreview
            XCODE_PRODUCT_TYPE com.apple.product-type.app-extension
            XCODE_ATTRIBUTE_APPLICATION_EXTENSION_API_ONLY YES
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/quicklook/Preview-Info.plist
    )
    target_link_libraries(tivars_quicklook_preview_extension PRIVATE
            "-framework AppKit"
            "-framework QuickLookUI"
            "-framework UniformTypeIdentifiers"
    )

    add_executable(tivars_quicklook_thumbnail_extension MACOSX_BUNDLE
            $<TARGET_OBJECTS:tivars_quicklook_support>
            quicklook/ThumbnailProvider.mm
            quicklook/ExtensionMain.m
            programs_tokens.csv
    )
    set_target_properties(tivars_quicklook_thumbnail_extension PROPERTIES
            BUNDLE_EXTENSION appex
            OUTPUT_NAME TIVarsQuickLookThumbnail
            XCODE_PRODUCT_TYPE com.apple.product-type.app-extension
            XCODE_ATTRIBUTE_APPLICATION_EXTENSION_API_ONLY YES
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/quicklook/Thumbnail-Info.plist
    )
    target_link_libraries(tivars_quicklook_thumbnail_extension PRIVATE
            "-framework AppKit"
            "-framework QuickLookThumbnailing"
    )

    add_executable(tivars_quicklook_app MACOSX_BUNDLE
            quicklook/main.m
            quicklook/AppDelegate.m
    )
    set_target_properties(tivars_quicklook_app PROPERTIES
            OUTPUT_NAME TIVarsQuickLook
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/quicklook/App-Info.plist
    )
    target_link_libraries(tivars_quicklook_app PRIVATE "-framework AppKit")

    add_dependencies(tivars_quicklook_app
            tivars_quicklook_preview_extension
            tivars_quicklook_thumbnail_extension
    )

    set(TIVARS_QUICKLOOK_PLUGINS_DIR $<TARGET_BUNDLE_CONTENT_DIR:tivars_quicklook_app>/PlugIns)
    set(TIVARS_QUICKLOOK_EMBEDDED_PREVIEW ${TIVARS_QUICKLOOK_PLUGINS_DIR}/TIVarsQuickLookPreview.appex)
    set(TIVARS_QUICKLOOK_EMBEDDED_THUMBNAIL ${TIVARS_QUICKLOOK_PLUGINS_DIR}/TIVarsQuickLookThumbnail.appex)

    set(TIVARS_QUICKLOOK_CODESIGN_ARGS
            ${CODESIGN_EXECUTABLE} --force --verify --options runtime --timestamp --sign "${TIVARS_QUICKLOOK_CODESIGN_IDENTITY}"
    )

    add_custom_target(tivars_quicklook ALL
            DEPENDS tivars_quicklook_app
            COMMAND ${CMAKE_COMMAND} -E make_directory ${TIVARS_QUICKLOOK_PLUGINS_DIR}
            COMMAND ${CMAKE_COMMAND} -E rm -rf ${TIVARS_QUICKLOOK_EMBEDDED_PREVIEW}
            COMMAND ${CMAKE_COMMAND} -E rm -rf ${TIVARS_QUICKLOOK_EMBEDDED_THUMBNAIL}
            COMMAND ${TIVARS_QUICKLOOK_CODESIGN_ARGS} --entitlements ${CMAKE_CURRENT_SOURCE_DIR}/quicklook/Preview.entitlements  $<TARGET_BUNDLE_DIR:tivars_quicklook_preview_extension>
            COMMAND ${TIVARS_QUICKLOOK_CODESIGN_ARGS} --entitlements ${CMAKE_CURRENT_SOURCE_DIR}/quicklook/Thumbnail.entitlements $<TARGET_BUNDLE_DIR:tivars_quicklook_thumbnail_extension>
            COMMAND mv $<TARGET_BUNDLE_DIR:tivars_quicklook_preview_extension> ${TIVARS_QUICKLOOK_EMBEDDED_PREVIEW}
            COMMAND mv $<TARGET_BUNDLE_DIR:tivars_quicklook_thumbnail_extension> ${TIVARS_QUICKLOOK_EMBEDDED_THUMBNAIL}
            COMMAND ${TIVARS_QUICKLOOK_CODESIGN_ARGS} --entitlements ${CMAKE_CURRENT_SOURCE_DIR}/quicklook/App.entitlements $<TARGET_BUNDLE_DIR:tivars_quicklook_app>
            VERBATIM
    )
endif()
