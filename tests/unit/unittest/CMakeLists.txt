if (NOT QTC_CLANG_BUILDMODE_MATCH)
    message(STATUS "Skipping unittest due to build mode mismatch")
    return()
endif()

find_package(Googletest MODULE)
find_package(GoogleBenchmark MODULE)

if (NOT Googletest_FOUND)
  message(STATUS "Googletest was not found. Please set GOOGLETEST_DIR (CMake or Environment) variable.")
  message(STATUS "Have a look at cmake/FindGoogletest.cmake file for more details.")
  message(STATUS "unittest module will be skipped.")
  return()
endif()

if (MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
elseif (MINGW)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mbig-obj")
endif()

add_qtc_test(unittest GTEST
  INCLUDES
    BEFORE "../mockup"
    BEFORE "../mockup/qmldesigner/designercore/include"
  DEPENDS
    Qt5::Core Qt5::Network Qt5::Widgets
    Qt5::Xml Qt5::Concurrent Qt5::Qml Qt5::Gui
    Qt6Core5Compat
    Googletest
  DEFINES
    QT_NO_CAST_TO_ASCII
    QT_RESTRICTED_CAST_FROM_ASCII
    UNIT_TESTS
    DONT_CHECK_MESSAGE_COUNTER
    QTC_RESOURCE_DIR="${CMAKE_CURRENT_LIST_DIR}/../../../share/qtcreator"
    TESTDATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
    ECHOSERVER="$<TARGET_FILE_DIR:echo>/echo"
    CPPTOOLS_JSON="${CMAKE_CURRENT_BINARY_DIR}/CppTools.json"
  SOURCES
    abstractviewmock.h
    clientserverinprocess-test.cpp
    clientserveroutsideprocess-test.cpp
    compare-operators.h
    compileroptionsbuilder-test.cpp
    conditionally-disabled-tests.h
    cppprojectfilecategorizer-test.cpp
    cppprojectinfogenerator-test.cpp
    cppprojectpartchooser-test.cpp
    dummyclangipcclient.h
    dynamicastmatcherdiagnosticcontainer-matcher.h
    eventspy.cpp eventspy.h
    fakeprocess.cpp fakeprocess.h
    filesystem-utilities.h
    googletest.h
    google-using-declarations.h
    gtest-creator-printing.cpp gtest-creator-printing.h
    gtest-llvm-printing.h
    gtest-qt-printing.cpp gtest-qt-printing.h
    gtest-std-printing.h
    headerpathfilter-test.cpp
    highlightingresultreporter-test.cpp
    lastchangedrowid-test.cpp
    lineprefixer-test.cpp
    matchingtext-test.cpp
    mimedatabase-utilities.cpp mimedatabase-utilities.h
    mockclangcodemodelclient.h
    mockclangcodemodelserver.h
    mockcppmodelmanager.h
    mockfutureinterface.h
    mockmutex.h
    mockqfilesystemwatcher.h
    mockqueue.h
    mocksqlitestatement.h
    mocksqlitetransactionbackend.h
    mocksyntaxhighligher.h
    mocktimer.cpp mocktimer.h
    nodelistproperty-test.cpp
    processcreator-test.cpp
    processevents-utilities.cpp processevents-utilities.h
    readandwritemessageblock-test.cpp
    rundocumentparse-utility.h
    sizedarray-test.cpp
    smallstring-test.cpp
    sourcerangecontainer-matcher.h
    spydummy.cpp spydummy.h
    sqlitealgorithms-test.cpp
    sqliteindex-test.cpp
    sqliteteststatement.h
    sqlitetransaction-test.cpp
    task.cpp
    tokenprocessor-test.cpp
    unittests-main.cpp
    unittest-utility-functions.h
    utf8-test.cpp
    sqlitecolumn-test.cpp
    sqlitedatabasebackend-test.cpp
    sqlitedatabase-test.cpp
    sqlitesessions-test.cpp
    sqlitestatement-test.cpp
    sqlitetable-test.cpp
    sqlstatementbuilder-test.cpp
    createtablesqlstatementbuilder-test.cpp
    sqlitevalue-test.cpp
    asynchronousimagecache-test.cpp
    synchronousimagecache-test.cpp
    imagecachegenerator-test.cpp
    imagecachestorage-test.cpp
    sqlitedatabasemock.h
    sqlitereadstatementmock.cpp sqlitereadstatementmock.h
    sqlitereadwritestatementmock.cpp
    sqlitestatementmock.h
    sqlitetransactionbackendmock.h
    sqlitewritestatementmock.cpp sqlitewritestatementmock.h
    notification.h
    mocktimestampprovider.h
    imagecachecollectormock.h
    mockimagecachegenerator.h
    mockimagecachestorage.h
)

if (NOT TARGET unittest)
    return()
endif()

function(extend_qtc_test_with_target_sources target)
  cmake_parse_arguments(_arg "" "" "DEFINES;INCLUDES" ${ARGN})

  get_target_property(${target}Sources ${target} SOURCES)
  # work around issue with CMake < 3.14 where target sources can contain
  # $<TARGET_OBJECTS:...>
  list(FILTER ${target}Sources EXCLUDE REGEX "^\\$<TARGET_OBJECTS:.*")

  get_target_property(${target}SourcesDir ${target} SOURCES_DIR)
  extend_qtc_test(unittest
    SOURCES_PREFIX "${${target}SourcesDir}"
    SOURCES ${${target}Sources}
    DEFINES
      $<TARGET_PROPERTY:${target},INTERFACE_COMPILE_DEFINITIONS>
      ${_arg_DEFINES}
    INCLUDES
      $<TARGET_PROPERTY:${target},INTERFACE_INCLUDE_DIRECTORIES>
      ${_arg_INCLUDES}
  )
endfunction()

if (NOT TARGET clangbackend_lib)
  add_subdirectory(../../../src/tools/clangbackend/source ${CMAKE_CURRENT_BINARY_DIR}/clangbackend)
endif()
extend_qtc_test(unittest DEPENDS clangbackend_lib)

if (NOT TARGET Sqlite)
  add_subdirectory(../../../src/libs/sqlite ${CMAKE_CURRENT_BINARY_DIR}/sqlite)
endif()
extend_qtc_test_with_target_sources(Sqlite
  DEFINES _HAVE_SQLITE_CONFIG_H
)

if (APPLE)
  extend_qtc_test(unittest DEFINES _BSD_SOURCE)
elseif (UNIX)
  extend_qtc_test(unittest DEFINES _POSIX_C_SOURCE=200809L _GNU_SOURCE _DEFAULT_SOURCE)
endif()

# Do not work on the source directory data
add_custom_command(TARGET unittest POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/data"
          "${CMAKE_CURRENT_BINARY_DIR}/data"
)

# create fake CppTools.json for the mime type definitions
file(READ "../../../src/plugins/cpptools/CppTools.json.in" plugin_json_in)
string(REPLACE "\\\"" "\"" plugin_json_in ${plugin_json_in})
string(REPLACE "\\'" "'" plugin_json_in ${plugin_json_in})
string(REPLACE "$$QTCREATOR_VERSION" "${IDE_VERSION}" plugin_json_in ${plugin_json_in})
string(REPLACE "$$QTCREATOR_COMPAT_VERSION" "${IDE_VERSION_COMPAT}" plugin_json_in ${plugin_json_in})
string(REPLACE "$$QTCREATOR_COPYRIGHT_YEAR" "${IDE_COPYRIGHT_YEAR}" plugin_json_in ${plugin_json_in})
string(REPLACE "$$dependencyList" "\"Dependencies\" : []" plugin_json_in ${plugin_json_in})
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/CppTools.json" ${plugin_json_in})

extend_qtc_test(unittest
  CONDITION TARGET libclang
  INCLUDES "${CLANG_INCLUDE_DIRS}"
  DEFINES CLANG_UNIT_TESTS
  DEPENDS libclang
  SOURCES
    activationsequencecontextprocessor-test.cpp
    activationsequenceprocessor-test.cpp
    chunksreportedmonitor.cpp
    clangasyncjob-base.cpp
    clangcodecompleteresults-test.cpp
    clangcodemodelserver-test.cpp
    clangcompletecodejob-test.cpp
    clangcompletioncontextanalyzer-test.cpp
    clangdiagnosticfilter-test.cpp
    clangdocumentprocessors-test.cpp
    clangdocumentprocessor-test.cpp
    clangdocuments-test.cpp
    clangdocumentsuspenderresumer-test.cpp
    clangdocument-test.cpp
    clangfixitoperation-test.cpp
    clangfollowsymbol-test.cpp
    clangisdiagnosticrelatedtolocation-test.cpp
    clangjobqueue-test.cpp
    clangjobs-test.cpp
    clangparsesupportivetranslationunitjob-test.cpp
    clangreferencescollector-test.cpp
    clangrequestannotationsjob-test.cpp
    clangrequestreferencesjob-test.cpp
    clangresumedocumentjob-test.cpp
    clangstring-test.cpp
    clangsupportivetranslationunitinitializer-test.cpp
    clangsuspenddocumentjob-test.cpp
    clangtooltipinfo-test.cpp
    clangtranslationunits-test.cpp
    clangtranslationunit-test.cpp
    clangupdateannotationsjob-test.cpp
    codecompleter-test.cpp
    codecompletionsextractor-test.cpp
    completionchunkstotextconverter-test.cpp
    cursor-test.cpp
    diagnosticset-test.cpp
    diagnostic-test.cpp
    fixit-test.cpp
    gtest-clang-printing.cpp gtest-clang-printing.h
    readexporteddiagnostics-test.cpp
    senddocumenttracker-test.cpp
    skippedsourceranges-test.cpp
    sourcelocation-test.cpp
    sourcerange-test.cpp
    token-test.cpp
    translationunitupdater-test.cpp
    unsavedfile-test.cpp
    unsavedfiles-test.cpp
    utf8positionfromlinecolumn-test.cpp
    chunksreportedmonitor.h
    clangasyncjob-base.h
    clangcompareoperators.h
    diagnosticcontainer-matcher.h
)

extend_qtc_test(unittest
  CONDITION TARGET GoogleBenchmark
  DEPENDS GoogleBenchmark
  SOURCES
    smallstring-benchmark.cpp
)

finalize_qtc_gtest(unittest ".c$")

# Path needs to be before CppTools
target_include_directories(unittest
  PRIVATE
    BEFORE ../../../src/tools/clangbackend/source
    BEFORE ../../../src/plugins
)

# QmlDesigner tests

set(QmlDesignerDir ../../../src/plugins/qmldesigner)
extend_qtc_test(unittest
  INCLUDES
    "${QmlDesignerDir}"
    "${QmlDesignerDir}/designercore"
    "${QmlDesignerDir}/designercore/include"
    "${QmlDesignerDir}/designercore/imagecache"
    "${QmlDesignerDir}/../../../share/qtcreator/qml/qmlpuppet/interfaces"
    "${QmlDesignerDir}/../../../share/qtcreator/qml/qmlpuppet/types"
  DEFINES
    QMLDESIGNER_TEST DESIGNER_STATIC_CORE_LIBRARY
  SOURCES_PREFIX
    "${QmlDesignerDir}"
  SOURCES
    components/listmodeleditor/listmodeleditormodel.cpp components/listmodeleditor/listmodeleditormodel.h
)
extend_qtc_test(unittest
  SOURCES_PREFIX "${QmlDesignerDir}/designercore"
  SOURCES
    ../../../../share/qtcreator/qml/qmlpuppet/interfaces/commondefines.h
    ../components/listmodeleditor/listmodeleditormodel.cpp
    ../components/listmodeleditor/listmodeleditormodel.h
    exceptions/exception.cpp
    exceptions/invalidargumentexception.cpp
    exceptions/invalididexception.cpp
    exceptions/invalidmetainfoexception.cpp
    exceptions/invalidmodelnodeexception.cpp
    exceptions/invalidmodelstateexception.cpp
    exceptions/invalidpropertyexception.cpp
    exceptions/invalidqmlsourceexception.cpp
    exceptions/invalidreparentingexception.cpp
    exceptions/invalidslideindexexception.cpp
    exceptions/notimplementedexception.cpp
    exceptions/removebasestateexception.cpp
    exceptions/rewritingexception.cpp
    imagecache/asynchronousimagecache.cpp
    imagecache/imagecachecollectorinterface.h
    imagecache/imagecachegenerator.cpp
    imagecache/imagecachegenerator.h
    imagecache/imagecachegeneratorinterface.h
    imagecache/imagecachestorage.h
    imagecache/imagecachestorageinterface.h
    imagecache/synchronousimagecache.cpp
    imagecache/timestampproviderinterface.h
    include/abstractproperty.h
    include/abstractview.h
    include/asynchronousimagecache.h
    include/asynchronousimagecacheinterface.h
    include/bindingproperty.h
    include/imagecacheauxiliarydata.h
    include/import.h
    include/model.h
    include/modelnode.h
    include/nodeabstractproperty.h
    include/nodelistproperty.h
    include/nodeproperty.h
    include/projectstorageids.h
    include/qmldesignercorelib_global.h
    include/signalhandlerproperty.h
    include/synchronousimagecache.h
    include/variantproperty.h
    model/abstractproperty.cpp
    model/abstractview.cpp
    model/annotation.cpp
    model/bindingproperty.cpp
    model/import.cpp
    model/internalbindingproperty.cpp
    model/internalbindingproperty.h
    model/internalnode.cpp
    model/internalnode_p.h
    model/internalnodeabstractproperty.cpp
    model/internalnodeabstractproperty.h
    model/internalnodelistproperty.cpp
    model/internalnodelistproperty.h
    model/internalnodeproperty.cpp
    model/internalnodeproperty.h
    model/internalproperty.cpp
    model/internalproperty.h
    model/internalsignalhandlerproperty.cpp
    model/internalsignalhandlerproperty.h
    model/internalvariantproperty.cpp
    model/internalvariantproperty.h
    model/model.cpp
    model/model_p.h
    model/modelnode.cpp
    model/nodeabstractproperty.cpp
    model/nodelistproperty.cpp
    model/nodeproperty.cpp
    model/signalhandlerproperty.cpp
    model/variantproperty.cpp
    projectstorage/projectstorage.h
    projectstorage/projectstoragesqlitefunctionregistry.cpp
    projectstorage/projectstoragesqlitefunctionregistry.h
    projectstorage/projectstoragetypes.h
    projectstorage/sourcepath.h
    projectstorage/sourcepathcache.h
    projectstorage/sourcepathcache.h
    projectstorage/sourcepathcachetypes.h
    projectstorage/sourcepathview.h
    projectstorage/storagecache.h
    projectstorage/storagecacheentry.h
    projectstorage/storagecachefwd.h
    rewritertransaction.cpp
    rewritertransaction.h
  EXPLICIT_MOC
    "../mockup/qmldesigner/designercore/include/nodeinstanceview.h"
    "../mockup/qmldesigner/designercore/include/rewriterview.h"
    "${QmlDesignerDir}/designercore/include/model.h"
)

extend_qtc_test(unittest
  SOURCES
    listmodeleditor-test.cpp
    projectstorage-test.cpp
    projectstoragesqlitefunctionregistry-test.cpp
    sourcepath-test.cpp
    sourcepathcache-test.cpp
    sourcepathview-test.cpp
    storagecache-test.cpp
)

# QmlDesigner tests END

if (NOT TARGET Utils)
  add_subdirectory(../../../src/libs/utils ${CMAKE_CURRENT_BINARY_DIR}/utils)
endif()
if (NOT TARGET CPlusPlus)
  add_subdirectory(../../../src/libs/3rdparty/cplusplus ${CMAKE_CURRENT_BINARY_DIR}/3rd_cplusplus)
  add_subdirectory(../../../src/libs/cplusplus ${CMAKE_CURRENT_BINARY_DIR}/cplusplus)
endif()
if (NOT TARGET ClangSupport)
  add_subdirectory(../../../src/libs/clangsupport ${CMAKE_CURRENT_BINARY_DIR}/clangsupport)
endif()

extend_qtc_test(unittest DEPENDS Utils CPlusPlus ClangSupport)

extend_qtc_test(unittest
  SOURCES_PREFIX ../../../src/plugins/clangcodemodel
  SOURCES
    clangactivationsequencecontextprocessor.cpp clangactivationsequencecontextprocessor.h
    clangactivationsequenceprocessor.cpp clangactivationsequenceprocessor.h
    clangcompletionchunkstotextconverter.cpp clangcompletionchunkstotextconverter.h
    clangcompletioncontextanalyzer.cpp clangcompletioncontextanalyzer.h
    clangdiagnosticfilter.cpp clangdiagnosticfilter.h
    clangfixitoperation.cpp clangfixitoperation.h
    clanghighlightingresultreporter.cpp clanghighlightingresultreporter.h
    clanguiheaderondiskmanager.cpp clanguiheaderondiskmanager.h
    clangisdiagnosticrelatedtolocation.h
)

find_package(yaml-cpp QUIET MODULE)

extend_qtc_test(unittest
  DEPENDS yaml-cpp
  DEFINES CLANGTOOLS_STATIC_LIBRARY
  SOURCES_PREFIX ../../../src/plugins/clangtools
  SOURCES
    clangtoolsdiagnostic.cpp
    clangtoolsdiagnostic.h
    clangtoolslogfilereader.cpp
    clangtoolslogfilereader.h
)

extend_qtc_test(unittest
  DEFINES DEBUGGER_STATIC_LIBRARY
  SOURCES_PREFIX ../../../src/plugins/debugger
  SOURCES
    analyzer/diagnosticlocation.cpp
    analyzer/diagnosticlocation.h
)

extend_qtc_test(unittest
  SOURCES_PREFIX ../../../src/plugins/compilationdatabaseprojectmanager
  SOURCES
    compilationdatabaseutils.cpp compilationdatabaseutils.h
)

extend_qtc_test(unittest
  SOURCES
    compilationdatabaseutils-test.cpp
)

extend_qtc_test(unittest
  SOURCES_PREFIX ../../../src/plugins/coreplugin
  DEFINES CORE_STATIC_LIBRARY
  SOURCES
    coreicons.cpp coreicons.h
    find/ifindfilter.cpp find/ifindfilter.h
    locator/ilocatorfilter.cpp locator/ilocatorfilter.h
)

extend_qtc_test(unittest
  SOURCES_PREFIX ../../../src/plugins/cpptools
  DEFINES CPPTOOLS_STATIC_LIBRARY
  SOURCES
    cppprojectfile.cpp cppprojectfile.h
    senddocumenttracker.cpp senddocumenttracker.h
    projectpart.cpp projectpart.h
    compileroptionsbuilder.cpp compileroptionsbuilder.h
    cppprojectfilecategorizer.cpp cppprojectfilecategorizer.h
    projectinfo.cpp projectinfo.h
    cppprojectinfogenerator.cpp cppprojectinfogenerator.cpp
    cppprojectpartchooser.cpp cppprojectpartchooser.h
    headerpathfilter.cpp headerpathfilter.h
)

extend_qtc_test(unittest
  SOURCES_PREFIX ../../../src/plugins/projectexplorer
  DEFINES PROJECTEXPLORER_STATIC_LIBRARY
  SOURCES
    projectmacro.cpp projectmacro.h
)

extend_qtc_test(unittest
  SOURCES_PREFIX ../../../src/plugins/clangformat
  CONDITION TARGET libclang
  DEPENDS ${CLANG_TOOLING_LIBS}
  SOURCES
    clangformatconstants.h
    clangformatbaseindenter.cpp clangformatbaseindenter.h
)

extend_qtc_test(unittest
  CONDITION TARGET libclang
  SOURCES
    clangformat-test.cpp
)
