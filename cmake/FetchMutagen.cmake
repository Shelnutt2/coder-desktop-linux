# Download Mutagen binary for file sync support.
# Disabled by default; enable with -DFETCH_MUTAGEN=ON
# or bundle your own binary and install it to ${CODER_LIB_DIR}.

option(FETCH_MUTAGEN "Download Mutagen binary during build" OFF)

set(MUTAGEN_VERSION "0.18.1" CACHE STRING "Mutagen version to download")

if(FETCH_MUTAGEN)
    # Determine platform
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        set(_mutagen_arch "amd64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        set(_mutagen_arch "arm64")
    else()
        message(FATAL_ERROR "Unsupported architecture for Mutagen: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    set(_mutagen_url "https://github.com/mutagen-io/mutagen/releases/download/v${MUTAGEN_VERSION}/mutagen_linux_${_mutagen_arch}_v${MUTAGEN_VERSION}.tar.gz")
    set(_mutagen_archive "${CMAKE_BINARY_DIR}/mutagen-${MUTAGEN_VERSION}.tar.gz")
    set(MUTAGEN_BINARY "${CMAKE_BINARY_DIR}/mutagen")

    if(NOT EXISTS "${MUTAGEN_BINARY}")
        message(STATUS "Downloading Mutagen v${MUTAGEN_VERSION} for ${_mutagen_arch}...")
        file(DOWNLOAD "${_mutagen_url}" "${_mutagen_archive}"
            STATUS _dl_status
            SHOW_PROGRESS
        )
        list(GET _dl_status 0 _dl_code)
        if(NOT _dl_code EQUAL 0)
            list(GET _dl_status 1 _dl_msg)
            message(FATAL_ERROR "Failed to download Mutagen: ${_dl_msg}")
        endif()

        # Extract just the 'mutagen' binary from the tarball
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xzf "${_mutagen_archive}" mutagen
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            RESULT_VARIABLE _extract_result
        )
        if(NOT _extract_result EQUAL 0)
            message(FATAL_ERROR "Failed to extract Mutagen binary")
        endif()

        # Make executable
        file(CHMOD "${MUTAGEN_BINARY}" PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
        )
        message(STATUS "Mutagen binary: ${MUTAGEN_BINARY}")
    endif()

    # Install to lib directory (MutagenDaemon searches ../lib/coder-desktop/)
    install(PROGRAMS "${MUTAGEN_BINARY}"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/coder-desktop"
        COMPONENT runtime
    )
endif()
