cmake_minimum_required(VERSION 3.16)

project(qTLF VERSION 0.9.3 DESCRIPTION "User-customizable contest logging software" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Sql Qml Quick QuickWidgets Network)

set(SOURCES
    src/main.cpp
    src/AppPaths.cpp            src/AppPaths.h
    src/Types.h
    src/ContestRules.cpp        src/ContestRules.h
    src/Database.cpp            src/Database.h
    src/QsoLogModel.cpp         src/QsoLogModel.h
    src/LogFilterProxy.cpp      src/LogFilterProxy.h
    src/ScoringEngine.cpp       src/ScoringEngine.h
    src/ContestBridge.h
    src/EntryBar.cpp            src/EntryBar.h
    src/BandSummaryWidget.cpp   src/BandSummaryWidget.h
    src/ContestSelectDialog.cpp src/ContestSelectDialog.h
    src/SettingsDialog.cpp      src/SettingsDialog.h
    src/Cabrillo.cpp            src/Cabrillo.h
    src/MainWindow.cpp          src/MainWindow.h
    src/MainWindowCw.cpp
    src/CwConfig.cpp            src/CwConfig.h
    src/Keyer.cpp               src/Keyer.h
    src/CwPanel.cpp             src/CwPanel.h
    src/AboutDialog.cpp         src/AboutDialog.h
    src/RemoteFiles.cpp         src/RemoteFiles.h
    src/CtyDat.cpp              src/CtyDat.h
    src/CallDatabase.cpp        src/CallDatabase.h
    src/CabrilloFieldsForm.cpp  src/CabrilloFieldsForm.h
    src/QsoChecks.cpp           src/QsoChecks.h
    src/Adif.cpp                src/Adif.h
    src/Translations.cpp        src/Translations.h
    src/TrxConfig.cpp           src/TrxConfig.h
    src/Trx.cpp                 src/Trx.h
    resources/qtlf.qrc
)

# ---- application icon
if(WIN32)
    # exe icon (Explorer, taskbar, shortcuts) + version info
    set(QTLF_ICO "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/qtlf.ico")
    set(QTLF_EXE "qtlf")
    set(QTLF_DESCRIPTION "qTLF - User-customizable contest logging software")
    configure_file(resources/qtlf.rc.in "${CMAKE_CURRENT_BINARY_DIR}/qtlf.rc" @ONLY)
    list(APPEND SOURCES "${CMAKE_CURRENT_BINARY_DIR}/qtlf.rc")
elseif(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE qtlf.icns)
    set(QTLF_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/qtlf.icns")
    set_source_files_properties(${QTLF_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    list(APPEND SOURCES ${QTLF_ICNS})
endif()

# ---- TRX control: Hamlib, loaded at run time (libhamlib-4.dll / libhamlib.so.4):
# no Hamlib SDK needed to build. -DQTLF_WITH_HAMLIB=OFF builds without TRX control.
option(QTLF_WITH_HAMLIB "TRX control through Hamlib (library loaded at run time)" ON)
if(QTLF_WITH_HAMLIB)
    list(APPEND SOURCES src/HamlibApi.cpp src/HamlibApi.h src/HamlibTrx.cpp src/HamlibTrx.h)
endif()

add_executable(qtlf ${SOURCES})

target_link_libraries(qtlf PRIVATE
    Qt6::Widgets Qt6::Sql Qt6::Qml Qt6::Quick Qt6::QuickWidgets Qt6::Network)

# ---- user-interface translations: translations/qtlf_<code>.ts -> qtlf_<code>.qm built into the
# program (resource :/i18n). English = the language of the sources. Needs Qt Linguist tools
# (Debian/Ubuntu: qt6-l10n-tools + qt6-tools-dev); without them qTLF is built in English only.
# New language: copy translations/qtlf_fr.ts to qtlf_<code>.ts, translate it with Qt Linguist,
# add it below. "cmake --build build --target update_translations" refreshes the .ts files.
find_package(Qt6 QUIET COMPONENTS LinguistTools)
if(Qt6LinguistTools_FOUND)
    file(GLOB QTLF_TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/qtlf_*.ts")
    qt_add_translations(qtlf TS_FILES ${QTLF_TS_FILES} RESOURCE_PREFIX "/i18n")
    message(STATUS "Translations: ${QTLF_TS_FILES}")
else()
    message(WARNING "Qt Linguist tools not found (sudo apt install qt6-l10n-tools qt6-tools-dev): English only")
endif()
if(QTLF_WITH_HAMLIB)
    target_compile_definitions(qtlf PRIVATE QTLF_HAVE_HAMLIB)
    target_link_libraries(qtlf PRIVATE ${CMAKE_DL_LIBS})     # dlopen (Linux / macOS)
endif()

target_compile_definitions(qtlf PRIVATE
    QTLF_VERSION="${PROJECT_VERSION}"
    # During development the rules are read straight from the source tree,
    # so editing rules/<CONTEST>/ needs no rebuild (Ctrl+R reloads them live).
    QTLF_DEV_RULES_DIR="${CMAKE_SOURCE_DIR}/rules")

if(WIN32)
    set_target_properties(qtlf PROPERTIES WIN32_EXECUTABLE ON)
elseif(APPLE)
    set_target_properties(qtlf PROPERTIES MACOSX_BUNDLE ON
        MACOSX_BUNDLE_BUNDLE_NAME qTLF
        MACOSX_BUNDLE_GUI_IDENTIFIER org.f8ehj.qtlf
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
endif()


# ---- Windows: target "deploy_qtlf" -> <build>/qtlf-deploy = qtlf.exe + Qt DLLs + QML modules
#      + rules/ + the Hamlib DLLs (from QTLF_HAMLIB_BIN, e.g. C:/hamlib-w64-4.5.5/bin).
#      That folder runs on its own and can be zipped / copied to any Windows PC.
if(WIN32)
    set(QTLF_HAMLIB_BIN "" CACHE PATH "bin folder of hamlib-w64 (libhamlib-4.dll…) copied by deploy_qtlf")
    set(_qt_bin "")
    if(TARGET Qt6::qmake)
        get_target_property(_qmake Qt6::qmake IMPORTED_LOCATION)
        if(_qmake)
            get_filename_component(_qt_bin "${_qmake}" DIRECTORY)
        endif()
    endif()
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
                 HINTS "${_qt_bin}" "${QT6_INSTALL_PREFIX}/bin" "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        set(_dd "${CMAKE_BINARY_DIR}/qtlf-deploy")
        set(_hamlib_copy "")
        if(QTLF_HAMLIB_BIN)
            file(GLOB _hamlib_dlls "${QTLF_HAMLIB_BIN}/*.dll")
            if(_hamlib_dlls)
                set(_hamlib_copy COMMAND ${CMAKE_COMMAND} -E copy ${_hamlib_dlls} "${_dd}/")
            else()
                message(WARNING "QTLF_HAMLIB_BIN=${QTLF_HAMLIB_BIN}: no DLL found there")
            endif()
        endif()
        add_custom_target(deploy_qtlf
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${_dd}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_dd}"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:qtlf>" "${_dd}/"
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    "$<$<CONFIG:Debug>:--debug>$<$<NOT:$<CONFIG:Debug>>:--release>"
                    --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/rules" "${_dd}/qtlf.exe"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/rules" "${_dd}/rules"
            ${_hamlib_copy}
            DEPENDS qtlf
            COMMENT "Deploying qTLF to ${_dd}"
            VERBATIM)
    endif()
endif()

install(TARGETS qtlf RUNTIME DESTINATION bin BUNDLE DESTINATION .)
# rules/ -> share/qtlf/rules: shipped contests are replaced as a whole when they changed (old copy
# kept in share/qtlf/rules-backup/), your own contest folders are never touched
install(CODE "set(QTLF_RULES_SRC \"${CMAKE_CURRENT_SOURCE_DIR}/rules\")")
install(SCRIPT cmake/InstallRules.cmake)
if(UNIX AND NOT APPLE)
    # desktop menu entry + icon theme (hicolor)
    # menu entry with the absolute path of the installed program (works even if bin/ is not in PATH)
    file(READ resources/qtlf.desktop _desktop)
    string(REPLACE "Exec=qtlf" "Exec=${CMAKE_INSTALL_PREFIX}/bin/qtlf" _desktop "${_desktop}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/qtlf.desktop" "${_desktop}")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qtlf.desktop" DESTINATION share/applications)
    foreach(s 16 24 32 48 64 128 256 512)
        install(FILES resources/icons/qtlf-${s}.png
                DESTINATION share/icons/hicolor/${s}x${s}/apps RENAME qtlf.png)
    endforeach()
endif()
