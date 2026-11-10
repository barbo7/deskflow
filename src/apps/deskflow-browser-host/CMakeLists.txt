# SPDX-FileCopyrightText: (C) 2026 Deskflow Devs
# SPDX-License-Identifier: MIT

set(target ${CMAKE_PROJECT_NAME}-browser-host)

add_executable(${target} "${target}.cpp")

target_link_libraries(${target} common Qt6::Core Qt6::Network)

install(
  TARGETS ${target}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(BUILD_OSX_BUNDLE)
  set_target_properties(${target} PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "@loader_path/../Libraries;@loader_path/../Frameworks"
    RUNTIME_OUTPUT_DIRECTORY $<TARGET_BUNDLE_CONTENT_DIR:${CMAKE_PROJECT_PROPER_NAME}>/MacOS
  )
endif()
