/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "SketchPlanCreator.h"
#include "SketchComplexItem.h"
#include "PlanMasterController.h"
#include "MissionController.h"
#include "QGCMAVLink.h"

SketchPlanCreator::SketchPlanCreator(PlanMasterController* planMasterController)
    : PlanCreator(planMasterController, SketchComplexItem::name, QStringLiteral("qrc:/maglab/SketchPlanCreator.png"), QGCMAVLink::allVehicleClasses())
{
}

void SketchPlanCreator::createPlan(const QGeoCoordinate& mapCenterCoord)
{
    _planMasterController->removeAll();
    _missionController->insertTakeoffItem(mapCenterCoord, -1);
    VisualMissionItem* sketch = _missionController->insertComplexMissionItem(SketchComplexItem::name, mapCenterCoord, -1);
    _missionController->insertLandItem(mapCenterCoord, -1);
    // Make the Sketch item current so its map-visual drawing layer is active.
    if (sketch) { _missionController->setCurrentPlanViewSeqNum(sketch->sequenceNumber(), true); }
}
