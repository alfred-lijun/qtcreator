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

#include "qttest_utils.h"
#include "qttesttreeitem.h"
#include "../autotestplugin.h"
#include "../testframeworkmanager.h"
#include "../testsettings.h"

#include <utils/algorithm.h>
#include <utils/environment.h>
#include <utils/qtcassert.h>

#include <QByteArrayList>
#include <QSet>

namespace Autotest {
namespace Internal {
namespace QTestUtils {

static const QByteArrayList valid = {"QTEST_MAIN", "QTEST_APPLESS_MAIN", "QTEST_GUILESS_MAIN"};

bool isQTestMacro(const QByteArray &macro)
{
    return valid.contains(macro);
}

QHash<Utils::FilePath, TestCases> testCaseNamesForFiles(ITestFramework *framework,
                                                        const Utils::FilePaths &files)
{
    QHash<Utils::FilePath, TestCases> result;
    TestTreeItem *rootNode = framework->rootNode();
    QTC_ASSERT(rootNode, return result);

    auto toTestCase = [](QtTestTreeItem *item){
        return TestCase{item->name(), item->runsMultipleTestcases()};
    };

    rootNode->forFirstLevelChildren([&](ITestTreeItem *child) {
        QtTestTreeItem *qtItem = static_cast<QtTestTreeItem *>(child);
        if (files.contains(child->filePath()))
            result[child->filePath()].append(toTestCase(qtItem));
        child->forFirstLevelChildren([&](ITestTreeItem *grandChild) {
            if (files.contains(grandChild->filePath()))
                result[grandChild->filePath()].append(toTestCase(qtItem));
        });
    });
    return result;
}

QMultiHash<Utils::FilePath, Utils::FilePath> alternativeFiles(ITestFramework *framework,
                                                              const Utils::FilePaths &files)
{
    QMultiHash<Utils::FilePath, Utils::FilePath> result;
    TestTreeItem *rootNode = framework->rootNode();
    QTC_ASSERT(rootNode, return result);

    rootNode->forFirstLevelChildren([&result, &files](ITestTreeItem *child) {
        const Utils::FilePath &baseFilePath = child->filePath();
        for (int childRow = 0, count = child->childCount(); childRow < count; ++childRow) {
            auto grandChild = static_cast<const QtTestTreeItem *>(child->childAt(childRow));
            const Utils::FilePath &filePath = grandChild->filePath();
            if (grandChild->inherited() && baseFilePath != filePath && files.contains(filePath)) {
                if (!result.contains(filePath, baseFilePath))
                    result.insert(filePath, baseFilePath);
            }
        }
    });
    return result;
}

QStringList filterInterfering(const QStringList &provided, QStringList *omitted, bool isQuickTest)
{
    static const QSet<QString> knownInterferingSingleOptions {
        "-txt", "-xml", "-csv", "-xunitxml", "-lightxml", "-silent", "-v1", "-v2", "-vs", "-vb",
        "-functions", "-datatags", "-nocrashhandler", "-callgrind", "-perf", "-perfcounterlist",
        "-tickcounter", "-eventcounter", "-help"
    };
    static const QSet<QString> knownInterferingOptionWithParameter = { "-o" };
    static const QSet<QString> knownAllowedOptionsWithParameter {
        "-eventdelay", "-keydelay", "-mousedelay", "-maxwarnings", "-perfcounter",
        "-minimumvalue", "-minimumtotal", "-iterations", "-median"
    };

    // handle Quick options as well
    static const QSet<QString> knownInterferingQuickOption = { "-qtquick1" };
    static const QSet<QString> knownAllowedQuickOptionsWithParameter {
        "-import", "-plugins", "-input", "-translation"
    };
    static const QSet<QString> knownAllowedSingleQuickOptions = { "-opengl", "-widgets" };

    QStringList allowed;
    auto it = provided.cbegin();
    auto end = provided.cend();
    for ( ; it != end; ++it) {
        QString currentOpt = *it;
        if (knownAllowedOptionsWithParameter.contains(currentOpt)) {
            allowed.append(currentOpt);
            ++it;
            QTC_ASSERT(it != end, return QStringList());
            allowed.append(*it);
        } else if (knownInterferingOptionWithParameter.contains(currentOpt)) {
            if (omitted) {
                omitted->append(currentOpt);
                ++it;
                QTC_ASSERT(it != end, return QStringList());
                omitted->append(*it);
            }
        } else if (knownInterferingSingleOptions.contains(currentOpt)) {
            if (omitted)
                omitted->append(currentOpt);
        } else if (isQuickTest) {
            if (knownAllowedQuickOptionsWithParameter.contains(currentOpt)) {
                allowed.append(currentOpt);
                ++it;
                QTC_ASSERT(it != end, return QStringList());
                allowed.append(*it);
            } else if (knownAllowedSingleQuickOptions.contains(currentOpt)) {
                allowed.append(currentOpt);
            } else if (knownInterferingQuickOption.contains(currentOpt)) {
                if (omitted)
                    omitted->append(currentOpt);
            }
        } else { // might be bad, but we cannot know anything
            allowed.append(currentOpt);
        }
    }
    return allowed;
}

Utils::Environment prepareBasicEnvironment(const Utils::Environment &env)
{
    Utils::Environment result(env);
    if (Utils::HostOsInfo::isWindowsHost()) {
        result.set("QT_FORCE_STDERR_LOGGING", "1");
        result.set("QT_LOGGING_TO_CONSOLE", "1");
    }
    const int timeout = AutotestPlugin::settings()->timeout;
    if (timeout > 5 * 60 * 1000) // Qt5.5 introduced hard limit, Qt5.6.1 added env var to raise this
        result.set("QTEST_FUNCTION_TIMEOUT", QString::number(timeout));
    return result;
}

} // namespace QTestUtils
} // namespace Internal
} // namespace Autotest
