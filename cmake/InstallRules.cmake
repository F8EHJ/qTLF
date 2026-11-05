# Installs the contest definitions (rules/) into <prefix>/share/qtlf/rules.
#
# * A contest shipped with qTLF (rules/SKM, rules/GENERIC…) is installed as one block: if the
#   installed folder differs from the shipped one in any way, the installed folder is moved to
#   <prefix>/share/qtlf/rules-backup/<ID>-<date-time> and the shipped folder is copied in full.
#   Old and new files of a contest are therefore never mixed (e.g. after a rules format change).
# * Contests that are not shipped with qTLF (your own folders) are never touched.
#
# QTLF_RULES_SRC is set by install(CODE ...) in the top-level CMakeLists.txt.
set(_root "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/qtlf")
set(_dst "${_root}/rules")
string(TIMESTAMP _stamp "%Y%m%d-%H%M%S")
file(MAKE_DIRECTORY "${_dst}")

file(GLOB _contests RELATIVE "${QTLF_RULES_SRC}" "${QTLF_RULES_SRC}/*")
foreach(_c IN LISTS _contests)
    if(NOT IS_DIRECTORY "${QTLF_RULES_SRC}/${_c}")
        continue()
    endif()
    set(_src "${QTLF_RULES_SRC}/${_c}")
    set(_inst "${_dst}/${_c}")

    # same content? (same file list and every file identical)
    set(_same FALSE)
    if(IS_DIRECTORY "${_inst}")
        file(GLOB_RECURSE _a RELATIVE "${_src}" "${_src}/*")
        file(GLOB_RECURSE _b RELATIVE "${_inst}" "${_inst}/*")
        list(SORT _a)
        list(SORT _b)
        if("${_a}" STREQUAL "${_b}")
            set(_same TRUE)
            foreach(_f IN LISTS _a)
                file(SHA256 "${_src}/${_f}" _h1)
                file(SHA256 "${_inst}/${_f}" _h2)
                if(NOT _h1 STREQUAL _h2)
                    set(_same FALSE)
                    break()
                endif()
            endforeach()
        endif()
    endif()

    if(_same)
        message(STATUS "Up-to-date: ${_inst}")
        continue()
    endif()
    if(IS_DIRECTORY "${_inst}")
        file(MAKE_DIRECTORY "${_root}/rules-backup")
        file(RENAME "${_inst}" "${_root}/rules-backup/${_c}-${_stamp}")
        message(STATUS "Old version moved to: ${_root}/rules-backup/${_c}-${_stamp}")
    endif()
    file(COPY "${_src}" DESTINATION "${_dst}")
    message(STATUS "Installing: ${_inst}")
endforeach()
