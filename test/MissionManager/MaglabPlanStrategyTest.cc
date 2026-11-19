#include "MaglabPlanStrategyTest.h"

#include "Planning/LLMPlan/CustomPlanStrategy.h"
#include "Planning/LLMPlan/FomPlanStrategy.h"
#include "Planning/LLMPlan/SpiralPlanStrategy.h"
#include "Planning/LLMPlan/SquarePlanStrategy.h"

#include <QJsonArray>
#include <cmath>
#include <memory>

void MaglabPlanStrategyTest::testGeneratedOutput()
{
    for (int strategyIndex = 0; strategyIndex < 4; ++strategyIndex) {
        for (int mode = 0; mode < 3; ++mode) {
            std::unique_ptr<LLMPlanStrategy> strategy;
            switch (strategyIndex) {
            case 0: strategy = std::make_unique<SpiralPlanStrategy>(); break;
            case 1: strategy = std::make_unique<FomPlanStrategy>(); break;
            case 2: strategy = std::make_unique<SquarePlanStrategy>(); break;
            default: strategy = std::make_unique<CustomPlanStrategy>(); break;
            }
            QJsonObject params = strategy->saveParams();
            params[QStringLiteral("altitudeMode")] = mode;
            if (strategyIndex == 3) {
                params[QStringLiteral("offsets")] = QJsonArray{
                    QJsonObject{{QStringLiteral("e"), 0}, {QStringLiteral("n"), 0}},
                    QJsonObject{{QStringLiteral("e"), 30}, {QStringLiteral("n"), 50}}};
            }
            QVERIFY(strategy->loadParams(params));
            const QGeoCoordinate center(36.0, 120.0, 50.0);
            strategy->recomputeGeometry(center);
            QVERIFY(strategy->path().size() >= 2);
            const int declaredCount = strategy->lastSequenceNumber();
            QObject owner;
            const auto frame = mode == 0 ? MAV_FRAME_GLOBAL_RELATIVE_ALT :
                               mode == 1 ? MAV_FRAME_GLOBAL : MAV_FRAME_GLOBAL_TERRAIN_ALT;
            const auto items = strategy->missionItems(80.0, frame, &owner);
            QCOMPARE(items.size(), declaredCount);
            QCOMPARE(strategy->lastSequenceNumber(), declaredCount);
            QVERIFY(!items.isEmpty());
            int navigationCount = 0;
            for (const auto* item : items) {
                QVERIFY(item);
                if (item->command() == MAV_CMD_NAV_WAYPOINT || item->command() == MAV_CMD_NAV_SPLINE_WAYPOINT) {
                    ++navigationCount;
                    QCOMPARE(item->frame(), frame);
                    QVERIFY(item->coordinate().isValid());
                    QVERIFY(std::isfinite(item->param7()));
                    QCOMPARE(item->param7(), 80.0);
                } else if (item->command() == MAV_CMD_DO_CHANGE_SPEED) {
                    QVERIFY(std::isfinite(item->param2()) && item->param2() > 0);
                } else {
                    QCOMPARE(item->command(), MAV_CMD_CONDITION_YAW);
                }
            }
            QVERIFY(navigationCount >= 2);
            const auto saved = strategy->saveParams();
            QVERIFY(strategy->loadParams(saved));
            strategy->recomputeGeometry(center);
            QCOMPARE(strategy->lastSequenceNumber(), declaredCount);
        }
    }
}

UT_REGISTER_TEST(MaglabPlanStrategyTest, TestLabel::Unit)
