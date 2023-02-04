
# message("\n------------------ Start configure install option ------------------")

include(GNUInstallDirs)

if(DEFINED USER_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/${USER_INSTALL_PREFIX})
else()
    set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install)
endif()

message("CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")

set(INSTALL_BIN       "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")            # ./install/bin
set(INSTALL_SBIN      "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SBINDIR}")           # ./install/sbin
set(INSTALL_LIB       "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")            # ./install/lib
set(INSTALL_INCLUDE   "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}")        # ./install/include
set(INSTALL_DOC       "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DOCDIR}")            # ./install/doc
set(INSTALL_CMAKE     "${CMAKE_INSTALL_PREFIX}/cmake")                              # ./install/cmake
set(INSTALL_EXAMPLE   "${CMAKE_INSTALL_PREFIX}/example")                            # ./install/example

# message("------------------ Stop configure install option ------------------\n")

function(InstallFunc target_name)
    install(
        TARGETS 
            ${target_name}
        EXPORT
            ${target_name}Targets
        LIBRARY 
            DESTINATION ${INSTALL_LIB}
            COMPONENT lib
        ARCHIVE 
            DESTINATION ${INSTALL_LIB}
            COMPONENT lib
        RUNTIME 
            DESTINATION ${INSTALL_BIN}
            COMPONENT bin
        PUBLIC_HEADER 
            DESTINATION ${INSTALL_INCLUDE}
            COMPONENT dev
        )

    install(
        EXPORT 
            ${target_name}Targets
        FILE
            ${target_name}Targets.cmake
        NAMESPACE
            ${target_name}::
        DESTINATION 
            ${INSTALL_CMAKE}
        )

    include(CMakePackageConfigHelpers)

    write_basic_package_version_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/${target_name}ConfigVersion.cmake
        VERSION 
            ${CMAKE_PROJECT_VERSION}
        COMPATIBILITY 
            SameMajorVersion
        )

    configure_package_config_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${target_name}Config.cmake.in
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${target_name}Config.cmake
        INSTALL_DESTINATION 
            ${INSTALL_CMAKE}
        )

    install(
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${target_name}ConfigVersion.cmake
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${target_name}Config.cmake
        DESTINATION
            ${INSTALL_CMAKE}
        )
endfunction()