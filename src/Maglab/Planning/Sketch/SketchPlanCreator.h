/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "PlanCreator.h"

/// @brief PlanCreator that registers the free-hand "Sketch" pattern.
class SketchPlanCreator : public PlanCreator
{
    Q_OBJECT

public:
    explicit SketchPlanCreator(PlanMasterController* planMasterController);

    void createPlan(const QGeoCoordinate& mapCenterCoord) final;
};
