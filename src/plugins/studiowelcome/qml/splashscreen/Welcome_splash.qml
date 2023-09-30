/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StudioFonts 1.0
import projectmodel 1.0
import usagestatistics 1.0

Rectangle {
    id: welcome_splash
    width: 800
    height: 480

    gradient: Gradient {
        orientation: Gradient.Horizontal

        GradientStop { position: 0.0; color: "#333d56" }
        GradientStop { position: 1.0; color: "#000728" }
    }

    signal goNext
    signal closeClicked
    signal configureClicked

    property alias doNotShowAgain: doNotShowCheckBox.checked
    property bool loadingPlugins: true

    // called from C++
    function onPluginInitialized(crashReportingEnabled: bool, crashReportingOn: bool)
    {
        loadingPlugins = false

        if (crashReportingEnabled) {
            var configureButton = "<a href='#' style='text-decoration:none;color:#ffff00'>"
                                  + qsTr("[Configure]") + "</a>";
            var settingPath = Qt.platform.os === "osx"
                              ? qsTr("Qt Creator > Preferences > Environment > System")
                              : qsTr("Tools > Options > Environment > System")
            var strOn = qsTr("Qt Design Studio collects crash reports for the sole purpose of fixing bugs. "
                        + "You can disable this feature under %1. %2").arg(settingPath).arg(configureButton)
           var strOff = qsTr("Qt Design Studio can collect crash reports for the sole purpose of fixing bugs. "
                        + "You can enable this feature under %1. %2").arg(settingPath).arg(configureButton)

            crash_reporting_text.text = crashReportingOn ? strOn : strOff;
        }
    }

    Image {
        id: logo
        x: 16
        y: 16
        source: "welcome_windows/logo.png"
    }

    Text {
        id: qt_design_studio
        x: 16
        y: 93
        width: 250
        height: 55
        color: "#4cd265"
        text: qsTr("Qt Design Studio")
        font.pixelSize: 36
        font.family: StudioFonts.titilliumWeb_light
    }

    Text {
        id: software_for_ui
        x: 16
        y: 141
        width: 250
        height: 30
        color: "#ffffff"
        text: qsTr("Software for UI and UX Designers")
        renderType: Text.QtRendering
        font.pixelSize: 18
        font.family: StudioFonts.titilliumWeb_light
    }

    Text {
        id: copyright
        x: 16
        y: 183
        width: 270
        height: 24
        color: "#ffffff"
        text: qsTr("Copyright 2008 - 2021 The Qt Company")
        font.pixelSize: 16
        font.family: StudioFonts.titilliumWeb_light
    }

    Text {
        id: all_rights_reserved
        x: 16
        y: 207
        width: 250
        height: 24
        color: "#ffffff"
        text: qsTr("All Rights Reserved")
        font.pixelSize: 16
        font.family: StudioFonts.titilliumWeb_light
    }

    Text {
        id: marketing_1
        x: 16
        y: 252
        width: 355
        height: 31
        color: "#ffffff"
        text: qsTr("Multi-paradigm language for creating highly dynamic applications.")
        wrapMode: Text.WordWrap
        font.family: StudioFonts.titilliumWeb_light
        font.pixelSize: 12
        font.wordSpacing: 0
    }

    Text {
        id: marketing_2
        x: 16
        y: 273
        width: 311
        height: 31
        color: "#ffffff"
        text: qsTr("Run your concepts and prototypes on your final hardware.")
        wrapMode: Text.WordWrap
        font.family: StudioFonts.titilliumWeb_light
        font.pixelSize: 12
        font.wordSpacing: 0
    }

    Text {
        id: marketing_3
        x: 16
        y: 294
        width: 311
        height: 31
        color: "#ffffff"
        text: qsTr("Seamless integration between designer and developer.")
        wrapMode: Text.WordWrap
        font.family: StudioFonts.titilliumWeb_light
        font.pixelSize: 12
        font.wordSpacing: 0
    }

    Text {
        id: crash_reporting_text
        color: "#ffffff"
        textFormat: Text.RichText
        x: 16
        y: 330
        width: 311
        wrapMode: Text.WordWrap
        font.family: StudioFonts.titilliumWeb_light
        font.pixelSize: 12
        font.wordSpacing: 0
        onLinkActivated: welcome_splash.configureClicked()

        MouseArea { // show hand cursor on link hover
            anchors.fill: parent
            acceptedButtons: Qt.NoButton // don't eat clicks on the Text
            cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
    }

    Dof_Effect {
        id: dof_Effect
        x: 358
        width: 442
        height: 480
        visible: true
        maskBlurSamples: 64
        maskBlurRadius: 32

        Splash_Image25d {
            id: animated_artwork
            x: 358
            y: 0
            width: 442
            height: 480
            clip: true
        }
    }

    Image {
        id: close_window
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 8
        width: 13
        height: 13
        fillMode: Image.PreserveAspectFit
        source: "welcome_windows/close.png"
        opacity: area.containsMouse ? 1 : 0.8

        MouseArea {
            id: area
            hoverEnabled: true
            anchors.fill: parent
            anchors.margins: -10
            onClicked: welcome_splash.closeClicked()
        }
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 16

        CheckBox {
            id: doNotShowCheckBox
            text: qsTr("Do not show this again")
            padding: 0
            spacing: 12

            contentItem: Text {
                text: doNotShowCheckBox.text
                color: "#ffffff"
                leftPadding: doNotShowCheckBox.indicator.width + doNotShowCheckBox.spacing
            }
        }

        CheckBox {
            id: usageStatisticCheckBox
            text: qsTr("Enable Usage Statistics")
            checked: usageStatisticModel.usageStatisticEnabled
            padding: 0
            spacing: 12

            onCheckedChanged: usageStatisticModel.setTelemetryEnabled(usageStatisticCheckBox.checked)

            contentItem: Text {
                text: usageStatisticCheckBox.text
                color: "#ffffff"
                leftPadding: usageStatisticCheckBox.indicator.width + usageStatisticCheckBox.spacing
            }
        }
    }

    RowLayout {
        x: 16
        y: 330
        visible: welcome_splash.loadingPlugins

        Text {
            id: text1
            color: "#ffffff"
            text: qsTr("%")
            font.pixelSize: 12

            RotationAnimator {
                target: text1
                from: 0
                to: 360
                duration: 1800
                running: true
                loops: -1
            }
        }

        Text {
            id: loading_progress
            color: "#ffffff"
            text: qsTr("Loading Plugins")
            font.family: StudioFonts.titilliumWeb_light
            font.pixelSize: 16
        }

        Text {
            id: text2
            color: "#ffffff"
            text: qsTr("%")
            font.pixelSize: 12

            RotationAnimator {
                target: text2
                from: 0
                to: 360
                duration: 2000
                running: true
                loops: -1
            }
        }
    }

    Text {
        id: all_rights_reserved1
        x: 16
        y: 75
        color: "#ffffff"
        text: qsTr("Community Edition")
        font.pixelSize: 13
        font.family: StudioFonts.titilliumWeb_light
        visible: projectModel.communityVersion

        ProjectModel {
            id: projectModel
        }

        UsageStatisticModel {
            id: usageStatisticModel
        }
    }
}
