DEFINES += VCSBASE_LIBRARY
include(../../qtcreatorplugin.pri)
HEADERS += vcsbase_global.h \
    vcsbaseconstants.h \
    vcsoutputformatter.h \
    wizard/vcsconfigurationpage.h \
    wizard/vcscommandpage.h \
    wizard/vcsjsextension.h \
    vcsplugin.h \
    vcsbaseplugin.h \
    baseannotationhighlighter.h \
    diffandloghighlighter.h \
    vcsbaseeditor.h \
    vcsbasesubmiteditor.h \
    basevcseditorfactory.h \
    submiteditorfile.h \
    basevcssubmiteditorfactory.h \
    submitfilemodel.h \
    commonvcssettings.h \
    nicknamedialog.h \
    vcsoutputwindow.h \
    cleandialog.h \
    vcscommand.h \
    vcsbaseclient.h \
    vcsbaseclientsettings.h \
    vcsbaseeditorconfig.h \
    submitfieldwidget.h \
    submiteditorwidget.h \
    vcsbasediffeditorcontroller.h

SOURCES += vcsplugin.cpp \
    vcsbaseplugin.cpp \
    vcsoutputformatter.cpp \
    wizard/vcsconfigurationpage.cpp \
    wizard/vcscommandpage.cpp \
    wizard/vcsjsextension.cpp \
    baseannotationhighlighter.cpp \
    diffandloghighlighter.cpp \
    vcsbaseeditor.cpp \
    vcsbasesubmiteditor.cpp \
    basevcseditorfactory.cpp \
    submiteditorfile.cpp \
    basevcssubmiteditorfactory.cpp \
    submitfilemodel.cpp \
    commonvcssettings.cpp \
    nicknamedialog.cpp \
    vcsoutputwindow.cpp \
    cleandialog.cpp \
    vcscommand.cpp \
    vcsbaseclient.cpp \
    vcsbaseclientsettings.cpp \
    vcsbaseeditorconfig.cpp \
    submitfieldwidget.cpp \
    submiteditorwidget.cpp \
    vcsbasediffeditorcontroller.cpp

RESOURCES += vcsbase.qrc

FORMS += \
    nicknamedialog.ui \
    cleandialog.ui \
    submiteditorwidget.ui

equals(TEST, 1): DEFINES += "SRC_DIR=\\\"$$IDE_SOURCE_TREE\\\""