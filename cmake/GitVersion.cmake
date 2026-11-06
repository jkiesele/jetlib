# Writes ${OUT} with the git version of ${SRC}; runs on every build, but only touches
# the file when the version changed, so unchanged builds do not recompile.
execute_process(COMMAND git describe --always --dirty --abbrev=12
                WORKING_DIRECTORY ${SRC}
                OUTPUT_VARIABLE JETLIB_GIT_VERSION
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
if(NOT JETLIB_GIT_VERSION)
  set(JETLIB_GIT_VERSION "unknown")
endif()
set(CONTENT "#define JETLIB_GIT_VERSION \"${JETLIB_GIT_VERSION}\"\n")
set(OLD "")
if(EXISTS ${OUT})
  file(READ ${OUT} OLD)
endif()
if(NOT "${OLD}" STREQUAL "${CONTENT}")
  file(WRITE ${OUT} "${CONTENT}")
endif()
