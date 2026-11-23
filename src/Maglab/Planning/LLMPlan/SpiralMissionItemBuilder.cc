/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "SpiralMissionItemBuilder.h"
#include "SpiralPlanStrategy.h"

#include <QtMath>

namespace {

constexpr double kSpeedEmitDelta = 0.4;

MissionItem* changeSpeed(double speed, QObject* parent)
{
    return new MissionItem(0, MAV_CMD_DO_CHANGE_SPEED, MAV_FRAME_MISSION,
                           1, speed, -1, 0, 0, 0, 0,
                           true, false, parent);
}

} // namespace

QList<MissionItem*> SpiralMissionItemBuilder::build(const SpiralPlanStrategy& strategy,
                                                    bool loiterTurns,
                                                    bool vtol,
                                                    double relAlt,
                                                    MAV_FRAME frame,
                                                    QObject* parent)
{
    QList<MissionItem*> items;
    if (strategy._displayPath.isEmpty() || strategy._arcTargetSpeed.isEmpty()) {
        return items;
    }

    const double cruiseSpeed = strategy._arcTargetSpeed.first();
    items.append(changeSpeed(cruiseSpeed, parent));

    if (loiterTurns) {
        if (vtol) {
            items.append(new MissionItem(0, MAV_CMD_DO_VTOL_TRANSITION, MAV_FRAME_MISSION,
                                         4.0, 0, 0, 0, 0, 0, 0,
                                         true, false, parent));
        }

        for (const LLMArcSegment& arc : strategy._arcSegments) {
            if (arc.sweepAngleDeg <= 0.0 || arc.radius <= 0.0 || !arc.center.isValid()) {
                continue;
            }
            // Every spiral segment is one exact half turn. The geometry keeps
            // adjacent centres/radii tangent, and the autopilot exits the
            // loiter on the tangent towards the next waypoint.
            constexpr double kHalfTurn = 0.5;
            const double radius = arc.clockwise ? arc.radius : -arc.radius;
            items.append(new MissionItem(0, MAV_CMD_NAV_LOITER_TURNS, frame,
                                         kHalfTurn, 1.0, radius, 1.0,
                                         arc.center.latitude(), arc.center.longitude(), relAlt,
                                         true, false, parent));
        }

        if (vtol) {
            items.append(new MissionItem(0, MAV_CMD_DO_VTOL_TRANSITION, MAV_FRAME_MISSION,
                                         3.0, 0, 0, 0, 0, 0, 0,
                                         true, false, parent));
        }
        return items;
    }

    const double spacing = strategy._spacing.rawValue().toDouble();
    const double entryAcceptance = qMin(qBound(1.2, spacing * 0.25, 8.0), 2.0);

    // The first point is a normal waypoint so the vehicle settles at the entry
    // point before starting the spline sequence.
    items.append(new MissionItem(0, MAV_CMD_NAV_WAYPOINT, frame,
                                 0.5, entryAcceptance, 0.0, 0.0,
                                 strategy._displayPath[0].latitude(),
                                 strategy._displayPath[0].longitude(), relAlt,
                                 true, false, parent));

    double commandedSpeed = cruiseSpeed;
    const double requestedSpeed = strategy.specifiedFlightSpeed();
    int previousArc = strategy._pointArc.value(0, 0);
    for (int i = 1; i < strategy._displayPath.size(); ++i) {
        const int arc = strategy._pointArc.value(i, previousArc);
        if (arc != previousArc && arc >= 0 && arc < strategy._arcTargetSpeed.size()) {
            const double nextSpeed = strategy._arcTargetSpeed[arc];
            const bool reachedRequested =
                    qAbs(nextSpeed - requestedSpeed) < 0.05 &&
                    qAbs(commandedSpeed - requestedSpeed) >= 0.05;
            if (qAbs(nextSpeed - commandedSpeed) >= kSpeedEmitDelta || reachedRequested) {
                items.append(changeSpeed(nextSpeed, parent));
                commandedSpeed = nextSpeed;
            }
            previousArc = arc;
        }

        items.append(new MissionItem(0, MAV_CMD_NAV_SPLINE_WAYPOINT, frame,
                                     0.0, 0.0, 0.0, 0.0,
                                     strategy._displayPath[i].latitude(),
                                     strategy._displayPath[i].longitude(), relAlt,
                                     true, false, parent));
    }

    return items;
}

int SpiralMissionItemBuilder::itemCount(const SpiralPlanStrategy& strategy,
                                        bool loiterTurns,
                                        bool vtol)
{
    if (loiterTurns) {
        return 1 + strategy._arcSegments.size() + (vtol ? 2 : 0);
    }
    // SpiralPlanStrategy keeps the exact spline count up to date whenever the
    // geometry is recomputed.
    return strategy._lastItemCount;
}
