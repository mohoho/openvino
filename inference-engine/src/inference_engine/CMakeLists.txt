# Copyright (C) 2018-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

set (TARGET_NAME "inference_engine")

if(THREADING STREQUAL "TBB" OR THREADING STREQUAL "TBB_AUTO")
    find_package(TBBBIND_2_5 QUIET)
    if (TBBBIND_2_5_FOUND)
        message(STATUS "Static tbbbind_2_5 package was found")
    endif()
endif()

file (GLOB LIBRARY_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/threading/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cpp/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cpp_interfaces/interface/*.cpp
      )

# Add include path to so_extension.hpp
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/ie_core.cpp
                    APPEND PROPERTY INCLUDE_DIRECTORIES "${OpenVINO_SOURCE_DIR}/ngraph/core/src/")

# TODO: WA for OneHot pass usage in reshape
set(LEGACY_SRC_ROOT "${IE_MAIN_SOURCE_DIR}/src/legacy_api/src")
set(LEGACY_LIBRARY_SHARED_SRCS
    "${LEGACY_SRC_ROOT}/transformations/convert_opset1_to_legacy/convert_one_hot_to_one_hot_ie.cpp"
    "${LEGACY_SRC_ROOT}/transformations/convert_opset1_to_legacy/convert_nms_5_to_legacy.cpp"
    "${LEGACY_SRC_ROOT}/ngraph_ops/nms_ie.cpp"
    "${LEGACY_SRC_ROOT}/ngraph_ops/onehot_ie.cpp")

set_source_files_properties(${LEGACY_LIBRARY_SHARED_SRCS} PROPERTIES
    COMPILE_DEFINITIONS "USE_STATIC_IE")

set(IE_STATIC_DEPENDENT_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/file_utils.cpp)
list(REMOVE_ITEM LIBRARY_SRC ${IE_STATIC_DEPENDENT_FILES})

if(ENABLE_IR_V7_READER)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/ie_network_reader.cpp PROPERTIES
        COMPILE_DEFINITIONS "ENABLE_IR_V7_READER")
endif()

file (GLOB LIBRARY_HEADERS
       ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
       ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
      )

if (LINUX)
    file (GLOB LIBRARY_SRC
          ${LIBRARY_SRC}
          ${CMAKE_CURRENT_SOURCE_DIR}/src/os/lin/*.cpp)
    file (GLOB LIBRARY_HEADERS
         ${LIBRARY_HEADERS}
         ${CMAKE_CURRENT_SOURCE_DIR}/src/os/lin/*.hpp)
endif()

if (WIN32)
    file (GLOB LIBRARY_SRC
         ${LIBRARY_SRC}
         ${CMAKE_CURRENT_SOURCE_DIR}/src/os/win/*.cpp)
    file (GLOB LIBRARY_HEADERS
         ${LIBRARY_HEADERS}
         ${CMAKE_CURRENT_SOURCE_DIR}/src/os/win/*.hpp)
endif()

if(ENABLE_SSE42)
    file(GLOB SSE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/cpu_x86_sse42/*.cpp)
    file(GLOB SSE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/cpu_x86_sse42/*.hpp)

    list(APPEND LIBRARY_HEADERS ${SSE_HEADERS})
    list(APPEND LIBRARY_SRC ${SSE_SRC})

    ie_sse42_optimization_flags(sse4_2_flags)
    set_source_files_properties(${SSE_SRC} PROPERTIES COMPILE_OPTIONS "${sse4_2_flags}")
    add_definitions(-DHAVE_SSE=1)

    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
        set_source_files_properties(${SSE_SRC} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    endif()
endif()

addVersionDefines(src/ie_version.cpp CI_BUILD_NUMBER)

set (PUBLIC_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

file (GLOB_RECURSE PUBLIC_HEADERS
       ${PUBLIC_HEADERS_DIR}/*.hpp
       ${PUBLIC_HEADERS_DIR}/*.h
      )

# Create named folders for the sources within the .vcproj
# Empty name lists them directly under the .vcproj

source_group("src" FILES ${LIBRARY_SRC})
source_group("include" FILES ${LIBRARY_HEADERS} ${PUBLIC_HEADERS})

# Plugin API library

add_library(${TARGET_NAME}_plugin_api INTERFACE)

target_include_directories(${TARGET_NAME}_plugin_api INTERFACE
    "${IE_MAIN_SOURCE_DIR}/src/plugin_api"
    $<TARGET_PROPERTY:${TARGET_NAME}_preproc,INTERFACE_INCLUDE_DIRECTORIES>
    ${PUBLIC_HEADERS_DIR}
    ${PUBLIC_HEADERS_DIR}/ie)

target_link_libraries(${TARGET_NAME}_plugin_api INTERFACE pugixml::static openvino::itt openvino::util)

set_ie_threading_interface_for(${TARGET_NAME}_plugin_api)

file(GLOB_RECURSE plugin_api_src "${IE_MAIN_SOURCE_DIR}/src/plugin_api/*.hpp"
                                 "${IE_MAIN_SOURCE_DIR}/src/plugin_api/*.h")

add_clang_format_target(${TARGET_NAME}_plugin_api_clang FOR_SOURCES ${plugin_api_src})

# Create object library

add_library(${TARGET_NAME}_obj OBJECT
            ${LIBRARY_SRC}
            ${LIBRARY_HEADERS}
            ${PUBLIC_HEADERS})

ie_faster_build(${TARGET_NAME}_obj
    UNITY PCH PRIVATE "src/precomp.hpp"
)

target_compile_definitions(${TARGET_NAME}_obj PRIVATE IMPLEMENT_INFERENCE_ENGINE_API
                                                      $<TARGET_PROPERTY:ngraph,INTERFACE_COMPILE_DEFINITIONS>
                                                      $<TARGET_PROPERTY:frontend_manager::static,INTERFACE_COMPILE_DEFINITIONS>)

target_include_directories(${TARGET_NAME}_obj SYSTEM PRIVATE $<TARGET_PROPERTY:ngraph,INTERFACE_INCLUDE_DIRECTORIES>
                                                             $<TARGET_PROPERTY:pugixml::static,INTERFACE_INCLUDE_DIRECTORIES>
                                                             $<TARGET_PROPERTY:frontend_manager::static,INTERFACE_INCLUDE_DIRECTORIES>
                                                             $<TARGET_PROPERTY:xbyak,INTERFACE_INCLUDE_DIRECTORIES>)

target_include_directories(${TARGET_NAME}_obj PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
                                                      "${CMAKE_CURRENT_BINARY_DIR}" # for static ie_plugins.hpp
                                                      "${IE_MAIN_SOURCE_DIR}/src/readers/ir_reader_v7" # for ie_ir_version.hpp
                                                      $<TARGET_PROPERTY:${TARGET_NAME}_legacy,INTERFACE_INCLUDE_DIRECTORIES>
                                                      $<TARGET_PROPERTY:${TARGET_NAME}_transformations,INTERFACE_INCLUDE_DIRECTORIES>
                                                      $<TARGET_PROPERTY:${TARGET_NAME}_plugin_api,INTERFACE_INCLUDE_DIRECTORIES>)

target_link_libraries(${TARGET_NAME}_obj PRIVATE ${TARGET_NAME}_reader_api openvino::itt openvino::util)

set_ie_threading_interface_for(${TARGET_NAME}_obj)
if (TBBBIND_2_5_FOUND)
    target_compile_definitions(${TARGET_NAME}_obj PRIVATE -DTBBBIND_2_5_AVAILABLE)
    target_link_libraries(${TARGET_NAME}_obj PRIVATE ${TBBBIND_2_5_IMPORTED_TARGETS})
endif()

# Create library file from object library

add_library(${TARGET_NAME}
            ${IE_STATIC_DEPENDENT_FILES}
            ${LEGACY_LIBRARY_SHARED_SRCS}
            ${vs_version_file}
            $<TARGET_OBJECTS:${TARGET_NAME}_obj>)

add_library(openvino::runtime ALIAS ${TARGET_NAME})

add_clang_format_target(${TARGET_NAME}_clang FOR_SOURCES ${IE_STATIC_DEPENDENT_FILES} ${LIBRARY_SRC} ${LIBRARY_HEADERS} ${PUBLIC_HEADERS})

ov_ncc_naming_style(FOR_TARGET ${TARGET_NAME}
                    INCLUDE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/openvino"
                    ADDITIONAL_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:ngraph,INTERFACE_INCLUDE_DIRECTORIES>)

ie_add_vs_version_file(NAME ${TARGET_NAME}
                       FILEDESCRIPTION "Inference Engine Core Runtime library")

set_ie_threading_interface_for(${TARGET_NAME})
if (TBBBIND_2_5_FOUND)
    target_compile_definitions(${TARGET_NAME} PRIVATE -DTBBBIND_2_5_AVAILABLE)
    target_link_libraries(${TARGET_NAME} PRIVATE ${TBBBIND_2_5_IMPORTED_TARGETS})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE pugixml::static openvino::itt openvino::util ${CMAKE_DL_LIBS} Threads::Threads
                                             frontend_manager::static inference_engine_transformations
                                     PUBLIC ngraph)

target_include_directories(${TARGET_NAME} INTERFACE
            $<BUILD_INTERFACE:${PUBLIC_HEADERS_DIR}>
            $<BUILD_INTERFACE:${PUBLIC_HEADERS_DIR}/ie>
    PRIVATE $<TARGET_PROPERTY:${TARGET_NAME}_plugin_api,INTERFACE_INCLUDE_DIRECTORIES>
            $<TARGET_PROPERTY:${TARGET_NAME}_legacy,INTERFACE_INCLUDE_DIRECTORIES>)

if(WIN32)
    set_target_properties(${TARGET_NAME} PROPERTIES COMPILE_PDB_NAME ${TARGET_NAME})
endif()

target_compile_definitions(${TARGET_NAME} PRIVATE IMPLEMENT_INFERENCE_ENGINE_API)

ie_register_plugins(MAIN_TARGET ${TARGET_NAME}
                    POSSIBLE_PLUGINS MultiDevicePlugin HeteroPlugin clDNNPlugin GNAPlugin MKLDNNPlugin myriadPlugin)

ie_add_api_validator_post_build_step(TARGET ${TARGET_NAME})

# Static library used for unit tests which are always built

if(BUILD_SHARED_LIBS)
    add_library(${TARGET_NAME}_s STATIC EXCLUDE_FROM_ALL
                $<TARGET_OBJECTS:${TARGET_NAME}_legacy_obj>
                $<TARGET_OBJECTS:${TARGET_NAME}_obj>
                ${IE_STATIC_DEPENDENT_FILES})

    set_ie_threading_interface_for(${TARGET_NAME}_s)
    if (TBBBIND_2_5_FOUND)
        target_compile_definitions(${TARGET_NAME}_s PRIVATE -DTBBBIND_2_5_AVAILABLE)
        target_link_libraries(${TARGET_NAME}_s PRIVATE ${TBBBIND_2_5_IMPORTED_TARGETS})
    endif()

    target_include_directories(${TARGET_NAME}_s PUBLIC
        $<TARGET_PROPERTY:inference_engine_plugin_api,INTERFACE_INCLUDE_DIRECTORIES>
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${IE_MAIN_SOURCE_DIR}/src/legacy_api/src")

    if(WIN32)
        set_target_properties(${TARGET_NAME}_s PROPERTIES COMPILE_PDB_NAME ${TARGET_NAME}_s)
    endif()

    target_link_libraries(${TARGET_NAME}_s PRIVATE openvino::itt ${CMAKE_DL_LIBS} ngraph
        frontend_manager::static inference_engine_transformations pugixml::static)

    target_compile_definitions(${TARGET_NAME}_s PUBLIC USE_STATIC_IE)

    set_target_properties(${TARGET_NAME}_s PROPERTIES
        EXCLUDE_FROM_ALL ON
        INTERPROCEDURAL_OPTIMIZATION_RELEASE ${ENABLE_LTO})
else()
    # for static OpenVINO build we can re-use inference_engine which is already static
    add_library(${TARGET_NAME}_s ALIAS ${TARGET_NAME})

    target_include_directories(${TARGET_NAME} PUBLIC
        $<TARGET_PROPERTY:inference_engine_plugin_api,INTERFACE_INCLUDE_DIRECTORIES>
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${IE_MAIN_SOURCE_DIR}/src/legacy_api/src")
endif()

# LTO

set_target_properties(${TARGET_NAME} ${TARGET_NAME}_obj
                      PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ${ENABLE_LTO})

# Export for build tree

export(TARGETS ${TARGET_NAME} NAMESPACE openvino::
       APPEND FILE "${CMAKE_BINARY_DIR}/OpenVINOTargets.cmake")

# Export for developer package

openvino_developer_export_targets(COMPONENT inference_engine TARGETS ${TARGET_NAME}_plugin_api)

# install TBB

list(APPEND core_components ngraph)
list(APPEND PATH_VARS "IE_INCLUDE_DIR" "IE_NGRAPH_DIR"
                      "IE_PARALLEL_CMAKE")

# define variables for InferenceEngineConfig.cmake
if(THREADING MATCHES "^(TBB|TBB_AUTO)$")
    set(IE_TBB_DIR "${TBB_DIR}")
    list(APPEND PATH_VARS "IE_TBB_DIR")
endif()

# install only downloaded TBB, system one is not installed
if(THREADING MATCHES "^(TBB|TBB_AUTO)$" AND TBBROOT MATCHES ${TEMP})
    ie_cpack_add_component(tbb REQUIRED)
    list(APPEND core_components tbb)

    install(DIRECTORY "${TBB}/include"
            DESTINATION runtime/3rdparty/tbb
            COMPONENT tbb)
    install(DIRECTORY "${TBB}/lib"
            DESTINATION runtime/3rdparty/tbb
            COMPONENT tbb)
    # Windows only
    if(EXISTS "${TBB}/bin")
        install(DIRECTORY "${TBB}/bin"
                DESTINATION runtime/3rdparty/tbb
                COMPONENT tbb)
    endif()
    install(FILES "${TBB}/LICENSE"
            DESTINATION runtime/3rdparty/tbb
            COMPONENT tbb)

    set(IE_TBB_DIR_INSTALL "3rdparty/tbb/cmake")

    install(FILES "${TBB}/cmake/TBBConfig.cmake"
                  "${TBB}/cmake/TBBConfigVersion.cmake"
            DESTINATION runtime/${IE_TBB_DIR_INSTALL}
            COMPONENT tbb)
endif()

# Install Inference Engine

ie_cpack_add_component(core REQUIRED DEPENDS ${core_components})
ie_cpack_add_component(core_dev REQUIRED core ngraph_dev)

install(DIRECTORY "${PUBLIC_HEADERS_DIR}/" DESTINATION runtime/include
        COMPONENT core_dev)

set_target_properties(${TARGET_NAME} PROPERTIES EXPORT_NAME runtime)
install(TARGETS ${TARGET_NAME} EXPORT OpenVINOTargets
        RUNTIME DESTINATION ${IE_CPACK_RUNTIME_PATH} COMPONENT core
        ARCHIVE DESTINATION ${IE_CPACK_ARCHIVE_PATH} COMPONENT core
        LIBRARY DESTINATION ${IE_CPACK_LIBRARY_PATH} COMPONENT core
        INCLUDES DESTINATION runtime/include
                             # TODO: remove later once samples are updated
                             runtime/include/ie)

install(FILES $<TARGET_FILE_DIR:${TARGET_NAME}>/plugins.xml
        DESTINATION ${IE_CPACK_RUNTIME_PATH}
        COMPONENT core)

# for InferenceEngineUnitTest
if(WIN32)
    install(FILES $<TARGET_FILE_DIR:${TARGET_NAME}>/plugins.xml
            DESTINATION tests COMPONENT tests EXCLUDE_FROM_ALL)
else()
    install(FILES $<TARGET_FILE_DIR:${TARGET_NAME}>/plugins.xml
            DESTINATION tests/lib COMPONENT tests EXCLUDE_FROM_ALL)
endif()

# Install cmake scripts

if(BUILD_SHARED_LIBS)
    install(EXPORT OpenVINOTargets
            FILE OpenVINOTargets.cmake
            NAMESPACE openvino::
            DESTINATION runtime/cmake
            COMPONENT core_dev)
endif()

set(IE_NGRAPH_DIR "${CMAKE_BINARY_DIR}/ngraph")
set(IE_INCLUDE_DIR "${PUBLIC_HEADERS_DIR}/ie")
set(IE_PARALLEL_CMAKE "${InferenceEngine_SOURCE_DIR}/cmake/ie_parallel.cmake")

configure_package_config_file("${OpenVINO_SOURCE_DIR}/cmake/templates/InferenceEngineConfig.cmake.in"
                              "${CMAKE_BINARY_DIR}/InferenceEngineConfig.cmake"
                               INSTALL_DESTINATION "${CMAKE_INSTALL_PREFIX}"
                               PATH_VARS ${PATH_VARS})

configure_package_config_file("${OpenVINO_SOURCE_DIR}/cmake/templates/OpenVINOConfig.cmake.in"
                              "${CMAKE_BINARY_DIR}/OpenVINOConfig.cmake"
                              INSTALL_DESTINATION "${CMAKE_INSTALL_PREFIX}"
                              PATH_VARS ${PATH_VARS})

set(IE_INCLUDE_DIR "include/ie")
set(IE_NGRAPH_DIR ".")
set(IE_TBB_DIR "${IE_TBB_DIR_INSTALL}")
set(IE_PARALLEL_CMAKE "cmake/ie_parallel.cmake")

configure_package_config_file("${OpenVINO_SOURCE_DIR}/cmake/templates/InferenceEngineConfig.cmake.in"
                              "${CMAKE_BINARY_DIR}/share/InferenceEngineConfig.cmake"
                              INSTALL_DESTINATION cmake
                              PATH_VARS ${PATH_VARS})

configure_package_config_file("${OpenVINO_SOURCE_DIR}/cmake/templates/OpenVINOConfig.cmake.in"
                              "${CMAKE_BINARY_DIR}/share/OpenVINOConfig.cmake"
                              INSTALL_DESTINATION share
                              PATH_VARS ${PATH_VARS})

configure_file("${OpenVINO_SOURCE_DIR}/cmake/templates/InferenceEngineConfig-version.cmake.in"
               "${CMAKE_BINARY_DIR}/InferenceEngineConfig-version.cmake" @ONLY)
configure_file("${OpenVINO_SOURCE_DIR}/cmake/templates/OpenVINOConfig-version.cmake.in"
               "${CMAKE_BINARY_DIR}/OpenVINOConfig-version.cmake" @ONLY)

install(FILES "${CMAKE_BINARY_DIR}/share/InferenceEngineConfig.cmake"
              "${CMAKE_BINARY_DIR}/InferenceEngineConfig-version.cmake"
              "${InferenceEngine_SOURCE_DIR}/cmake/ie_parallel.cmake"
        DESTINATION runtime/cmake
        COMPONENT core_dev)

install(FILES "${CMAKE_BINARY_DIR}/share/OpenVINOConfig.cmake"
              "${CMAKE_BINARY_DIR}/OpenVINOConfig-version.cmake"
        DESTINATION runtime/cmake
        COMPONENT core_dev)
