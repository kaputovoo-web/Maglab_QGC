#include "SingletonTest.h"

#include "MAVLinkProtocol.h"
#include "MultiVehicleManager.h"
#include "Vehicle.h"

SingletonTest::SingletonTest(QObject* parent)
    : QObject(parent)
{
    connect(MAVLinkProtocol::instance(), &MAVLinkProtocol::messageReceived,
            this, &SingletonTest::_mavlinkMessageReceived);
    connect(MultiVehicleManager::instance(), &MultiVehicleManager::activeVehicleChanged,
            this, [this](Vehicle*) {
        setTest1({});
        setTest2({});
        setIsboat(false);
    });
}

void SingletonTest::_mavlinkMessageReceived(LinkInterface* link, const mavlink_message_t& message)
{
    Q_UNUSED(link)
    const Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (!vehicle || message.sysid != vehicle->id()) {
        return;
    }
    if (message.msgid == MAVLINK_MSG_ID_GPS_RAW_INT) {
        mavlink_gps_raw_int_t gps;
        mavlink_msg_gps_raw_int_decode(&message, &gps);
        if (gps.fix_type < 2 || gps.lat < -900000000 || gps.lat > 900000000 ||
            gps.lon < -1800000000 || gps.lon > 1800000000) {
            setTest1({});
            setTest2({});
            return;
        }
        setTest1(QString::number(gps.lat / 1e7, 'f', 7));
        setTest2(QString::number(gps.lon / 1e7, 'f', 7));
    } else if (message.msgid == MAVLINK_MSG_ID_ADSB_VEHICLE) {
        mavlink_adsb_vehicle_t adsb;
        mavlink_msg_adsb_vehicle_decode(&message, &adsb);
        setIsboat(adsb.ICAO_address == 12345);
    }
}

void SingletonTest::setTest1(const QString& value)
{
    if (_test1 != value) {
        _test1 = value;
        emit test1Changed();
    }
}

void SingletonTest::setTest2(const QString& value)
{
    if (_test2 != value) {
        _test2 = value;
        emit test2Changed();
    }
}

void SingletonTest::setIsboat(bool value)
{
    if (_isboat != value) {
        _isboat = value;
        emit isboatChanged();
    }
}
