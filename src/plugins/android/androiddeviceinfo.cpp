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

#include "androiddeviceinfo.h"

namespace Android {

/**
 * Workaround for '????????????' serial numbers
 * @return ("-d") for buggy devices, ("-s", <serial no>) for normal
 */
QStringList AndroidDeviceInfo::adbSelector(const QString &serialNumber)
{
    if (serialNumber.startsWith(QLatin1String("????")))
        return QStringList("-d");
    return QStringList({"-s", serialNumber});
}

bool AndroidDeviceInfo::operator<(const AndroidDeviceInfo &other) const
{
    if (serialNumber.contains("????") != other.serialNumber.contains("????"))
        return !serialNumber.contains("????");
    if (type != other.type)
        return type == AndroidDeviceInfo::Hardware;
    if (sdk != other.sdk)
        return sdk < other.sdk;
    if (avdname != other.avdname)
        return avdname < other.avdname;

    return serialNumber < other.serialNumber;
}

bool AndroidDeviceInfo::operator==(const AndroidDeviceInfo &other) const
{
    return serialNumber == other.serialNumber && avdname == other.avdname && cpuAbi == other.cpuAbi
           && avdTarget == other.avdTarget && avdDevice == other.avdDevice
           && avdSkin == other.avdSkin && avdSdcardSize == other.avdSdcardSize && sdk == other.sdk
           && state == other.state && unauthorized == other.unauthorized && type == other.type;
}

QDebug &operator<<(QDebug &stream, const AndroidDeviceInfo &device)
{
    stream << "Type:" << (device.type == AndroidDeviceInfo::Emulator ? "Emulator" : "Device")
           << ", ABI:" << device.cpuAbi << ", Serial:" << device.serialNumber
           << ", Name:" << device.avdname << ", API:" << device.sdk
           << ", Authorised:" << !device.unauthorized;
    return stream;
}

} // namespace Android
