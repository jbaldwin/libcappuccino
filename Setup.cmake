# Search parent dirs for NAME, starting with CMAKE_CURRENT_SOURCE_DIR
# Stores result DIR/NAME in VAR
function(FindParentDir VAR NAME)
    set(DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    while(DIR)
        if(EXISTS "${DIR}/${NAME}")
            set(${VAR} "${DIR}/${NAME}" PARENT_SCOPE)
            break()
        endif()
        if("${DIR}" STREQUAL "/")
            message(FATAL_ERROR "FindParentDir: Unable to find: ${NAME}")
        endif()
        get_filename_component(DIR "${DIR}" DIRECTORY)
    endwhile()
endfunction(FindParentDir)

FindParentDir(MAKEDIR "make")
