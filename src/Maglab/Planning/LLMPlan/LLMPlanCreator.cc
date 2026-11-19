/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "LLMPlanCreator.h"
#include "LLMComplexItem.h"
#include "PlanMasterController.h"
#include "MissionController.h"
#include "QGCMAVLink.h"

LLMPlanCreator::LLMPlanCreator(PlanMasterController* planMasterController)
    : PlanCreator(planMasterController, LLMComplexItem::name, QStringLiteral("qrc:/maglab/LLMPlanCreator.png"), QGCMAVLink::allVehicleClasses())
{
}

void LLMPlanCreator::createPlan(const QGeoCoordinate& mapCenterCoord)
{
    _planMasterController->removeAll();
    VisualMissionItem* takeoffItem = _missionController->insertTakeoffItem(mapCenterCoord, -1);
    _missionController->insertComplexMissionItem(LLMComplexItem::name, mapCenterCoord, -1);
    if (takeoffItem) {
        _missionController->setCurrentPlanViewSeqNum(takeoffItem->sequenceNumber(), true);
    }
}
