/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QGeoCoordinate>
#include <QList>

/// @brief One circular arc segment produced by a geometry planner.
///
/// Geometry strategies expose these segments without knowing which MAVLink
/// command will eventually represent them. The vehicle-specific mission
/// builder can map an arc to either LOITER_TURNS or a sampled waypoint path.
struct LLMArcSegment
{
    QGeoCoordinate center;
    double         radius = 0.0;
    double         startAngleDeg = 0.0;
    double         sweepAngleDeg = 0.0;
    bool           clockwise = true;
};
