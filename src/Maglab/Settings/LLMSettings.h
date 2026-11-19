/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#ifndef LLMSettings_H
#define LLMSettings_H

#include "SettingsGroup.h"
#include <QtQml/qqmlregistration.h>

class LLMSettings : public SettingsGroup
{
    Q_OBJECT
    QML_ELEMENT

public:
    LLMSettings(QObject* parent = nullptr);
    DEFINE_SETTING_NAME_GROUP()

    DEFINE_SETTINGFACT(apiEndpoint)
    DEFINE_SETTINGFACT(modelName)
    DEFINE_SETTINGFACT(apiKey)
    DEFINE_SETTINGFACT(sttEndpoint)
    DEFINE_SETTINGFACT(sttModel)
    DEFINE_SETTINGFACT(sttApiKey)
    DEFINE_SETTINGFACT(systemPrompt)
    DEFINE_SETTINGFACT(defaultFlightSpeed)
    DEFINE_SETTINGFACT(defaultAltitudeMode)

    // Accessor convenient for C++ clients that want the defaults
    // returned by the Fact values declared above.
    double defaultFlightSpeedValue() const;
    int    defaultAltitudeModeValue() const;
};

#endif // LLMSettings_H
