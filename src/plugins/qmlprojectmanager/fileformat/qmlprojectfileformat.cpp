/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "qmlprojectfileformat.h"
#include "qmlprojectitem.h"
#include "filefilteritems.h"
#include <qmljs/qmljssimplereader.h>

#include <QVariant>
#include <QDebug>

enum {
    debug = false
};

namespace   {

QmlProjectManager::FileFilterBaseItem *setupFileFilterItem(QmlProjectManager::FileFilterBaseItem *fileFilterItem, const QmlJS::SimpleReaderNode::Ptr &node)
{
    const QVariant directoryProperty = node->property(QLatin1String("directory"));
    if (directoryProperty.isValid())
        fileFilterItem->setDirectory(directoryProperty.toString());

    const QVariant recursiveProperty = node->property(QLatin1String("recursive"));
    if (recursiveProperty.isValid())
        fileFilterItem->setRecursive(recursiveProperty.toBool());

    const QVariant pathsProperty = node->property(QLatin1String("paths"));
    if (pathsProperty.isValid())
        fileFilterItem->setPathsProperty(pathsProperty.toStringList());

    const QVariant filterProperty = node->property(QLatin1String("filter"));
    if (filterProperty.isValid())
        fileFilterItem->setFilter(filterProperty.toString());

    if (debug)
        qDebug() << "directory:" << directoryProperty << "recursive" << recursiveProperty << "paths" << pathsProperty;
    return fileFilterItem;
}

} //namespace

namespace QmlProjectManager {

QmlProjectItem *QmlProjectFileFormat::parseProjectFile(const Utils::FilePath &fileName, QString *errorMessage)
{
    QmlJS::SimpleReader simpleQmlJSReader;

    const QmlJS::SimpleReaderNode::Ptr rootNode = simpleQmlJSReader.readFile(fileName.toString());

    if (!simpleQmlJSReader.errors().isEmpty() || !rootNode->isValid()) {
        qWarning() << "unable to parse:" << fileName;
        qWarning() << simpleQmlJSReader.errors();
        if (errorMessage)
            *errorMessage = simpleQmlJSReader.errors().join(QLatin1String(", "));
        return nullptr;
    }

    if (rootNode->name() == QLatin1String("Project")) {
        auto projectItem = new QmlProjectItem;

        const QVariant mainFileProperty = rootNode->property(QLatin1String("mainFile"));
        if (mainFileProperty.isValid())
            projectItem->setMainFile(mainFileProperty.toString());

        const QVariant importPathsProperty = rootNode->property(QLatin1String("importPaths"));
        if (importPathsProperty.isValid())
            projectItem->setImportPaths(importPathsProperty.toStringList());

        const QVariant fileSelectorsProperty = rootNode->property(QLatin1String("fileSelectors"));
        if (fileSelectorsProperty.isValid())
            projectItem->setFileSelectors(fileSelectorsProperty.toStringList());

        const QVariant forceFreeTypeProperty = rootNode->property("forceFreeType");
        if (forceFreeTypeProperty.isValid())
            projectItem->setForceFreeType(forceFreeTypeProperty.toBool());

        const QVariant targetDirectoryPropery = rootNode->property("targetDirectory");
        if (targetDirectoryPropery.isValid())
            projectItem->setTargetDirectory(targetDirectoryPropery.toString());

        const QVariant qtForMCUProperty = rootNode->property("qtForMCUs");
        if (qtForMCUProperty.isValid() && qtForMCUProperty.toBool())
            projectItem->setQtForMCUs(qtForMCUProperty.toBool());

        const QVariant qt6ProjectProperty = rootNode->property("qt6Project");
        if (qt6ProjectProperty.isValid() && qt6ProjectProperty.toBool())
            projectItem->setQt6Project(qt6ProjectProperty.toBool());

        if (debug)
            qDebug() << "importPath:" << importPathsProperty << "mainFile:" << mainFileProperty;

        foreach (const QmlJS::SimpleReaderNode::Ptr &childNode, rootNode->children()) {
            if (debug)
                qDebug() << "reading type:" << childNode->name();

            if (childNode->name() == QLatin1String("QmlFiles")) {
                projectItem->appendContent(setupFileFilterItem(new FileFilterItem("*.qml"), childNode));
            } else if (childNode->name() == QLatin1String("JavaScriptFiles")) {
                projectItem->appendContent(setupFileFilterItem(new FileFilterItem("*.js"), childNode));
            } else if (childNode->name() == QLatin1String("ImageFiles")) {
                projectItem->appendContent(setupFileFilterItem(new ImageFileFilterItem(projectItem), childNode));
            } else if (childNode->name() == QLatin1String("CssFiles")) {
                projectItem->appendContent(setupFileFilterItem(new FileFilterItem("*.css"), childNode));
            } else if (childNode->name() == QLatin1String("FontFiles")) {
                projectItem->appendContent(setupFileFilterItem(new FileFilterItem("*.ttf;*.otf"), childNode));
            } else if (childNode->name() == QLatin1String("Files")) {
                projectItem->appendContent(setupFileFilterItem(new FileFilterBaseItem(), childNode));
            } else if (childNode->name() == "Environment") {
                const auto properties = childNode->properties();
                auto i = properties.constBegin();
                while (i != properties.constEnd()) {
                    projectItem->addToEnviroment(i.key(), i.value().toString());
                    ++i;
                }
            } else {
                qWarning() << "Unknown type:" << childNode->name();
            }
        }
        return projectItem;
    }

    if (errorMessage)
        *errorMessage = tr("Invalid root element: %1").arg(rootNode->name());

    return nullptr;
}

} // namespace QmlProjectManager
