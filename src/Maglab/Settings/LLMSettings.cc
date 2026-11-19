/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "LLMSettings.h"

#include <QQmlEngine>
#include <QtQml>

DECLARE_SETTINGGROUP(LLM, "LLM")
{
}

DECLARE_SETTINGSFACT(LLMSettings, apiEndpoint)
DECLARE_SETTINGSFACT(LLMSettings, modelName)
DECLARE_SETTINGSFACT(LLMSettings, apiKey)
DECLARE_SETTINGSFACT(LLMSettings, sttEndpoint)
DECLARE_SETTINGSFACT(LLMSettings, sttModel)
DECLARE_SETTINGSFACT(LLMSettings, sttApiKey)
DECLARE_SETTINGSFACT(LLMSettings, systemPrompt)
DECLARE_SETTINGSFACT(LLMSettings, defaultFlightSpeed)
DECLARE_SETTINGSFACT(LLMSettings, defaultAltitudeMode)

double LLMSettings::defaultFlightSpeedValue() const
{
    Fact* f = const_cast<LLMSettings*>(this)->defaultFlightSpeed();
    return f ? f->rawValue().toDouble() : 5.0;
}

int LLMSettings::defaultAltitudeModeValue() const
{
    Fact* f = const_cast<LLMSettings*>(this)->defaultAltitudeMode();
    return f ? f->rawValue().toInt() : 0;
}
