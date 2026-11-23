/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "MissionItem.h"

class SpiralPlanStrategy;

/// @brief Converts the shared spiral geometry into vehicle-specific mission items.
///
/// Geometry remains in SpiralPlanStrategy. This builder is the only place that
/// decides whether a spiral is represented by sampled spline waypoints or by
/// native LOITER_TURNS semicircles.
class SpiralMissionItemBuilder
{
public:
    static QList<MissionItem*> build(const SpiralPlanStrategy& strategy,
                                     bool loiterTurns,
                                     bool vtol,
                                     double relAlt,
                                     MAV_FRAME frame,
                                     QObject* parent);

    static int itemCount(const SpiralPlanStrategy& strategy,
                         bool loiterTurns,
                         bool vtol);
};
