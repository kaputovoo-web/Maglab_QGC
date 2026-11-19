import QtQuick
import QtQuick.Controls
import QGroundControl

/// Lightweight, dependency-free celebration burst for a successful plan upload.
Item {
    id: root
    anchors.fill: parent
    visible: active
    z: QGroundControl.zOrderTopMost + 20
    clip: true

    property bool active: false
    property var particles: []
    property bool _started: false
    property var _startTime: 0

    function start() {
        if (active) return
        var colors = ["#35D6C0", "#39A9FF", "#8DEBFF", "#F7C948", "#FF8A65", "#B388FF"]
        var list = []
        for (var i = 0; i < 64; ++i) {
            list.push({
                x: width * (0.35 + Math.random() * 0.30),
                y: height * 0.48,
                vx: (Math.random() - 0.5) * 520,
                vy: -180 - Math.random() * 420,
                size: 5 + Math.random() * 7,
                rotation: Math.random() * 360,
                spin: (Math.random() - 0.5) * 720,
                color: colors[Math.floor(Math.random() * colors.length)],
                round: Math.random() < 0.35
            })
        }
        particles = list
        _startTime = Date.now()
        active = true
        _started = true
        timer.start()
    }

    onWidthChanged: if (active) canvas.requestPaint()
    onHeightChanged: if (active) canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            if (!root.active) return
            var now = Date.now()
            var dt = Math.min(0.05, (now - root._startTime) / 1000)
            var alive = false
            for (var i = 0; i < root.particles.length; ++i) {
                var p = root.particles[i]
                p.vy += 620 * dt
                p.x += p.vx * dt
                p.y += p.vy * dt
                p.rotation += p.spin * dt
                if (p.y < height + 20) alive = true
                ctx.save()
                ctx.translate(p.x, p.y)
                ctx.rotate(p.rotation * Math.PI / 180)
                ctx.fillStyle = p.color
                if (p.round) {
                    ctx.beginPath()
                    ctx.arc(0, 0, p.size / 2, 0, Math.PI * 2)
                    ctx.fill()
                } else {
                    ctx.fillRect(-p.size / 2, -p.size / 4, p.size, p.size / 2)
                }
                ctx.restore()
            }
            if (!alive) {
                root.active = false
                timer.stop()
            }
        }
    }

    Timer {
        id: timer
        interval: 16
        repeat: true
        onTriggered: canvas.requestPaint()
    }
}
