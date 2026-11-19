# LLM Mission Pattern — Architecture

## Shell + Strategy Design

The LLM pattern uses a **shell + strategy** architecture under `src/MissionManager/LLMPlan/`.

### Classes

```
LLMPlanStrategy (abstract)
├── VoicePlanStrategy   (stub)
├── SpiralPlanStrategy  (Archimedean spiral)
└── FomPlanStrategy     (FOM calibration circles)

LLMComplexItem (ComplexMissionItem)  ← the shell
LLMPlanCreator (PlanCreator)         ← pattern menu registration
```

### How It Works

1. **LLMComplexItem** is registered as the "LLM" pattern in QGC's Plan View.
2. The editor shows a dropdown (planMode: Voice / Spiral / FOM).
3. Switching planMode activates the matching `LLMPlanStrategy` subclass.
4. All `ComplexMissionItem` / `VisualMissionItem` interface methods delegate to the active strategy.
5. Any Fact change or center move → `strategy->recomputeGeometry()` → map + editor update.

### Adding a New Sub-Mode

1. Create `FooPlanStrategy.h/.cc` subclassing `LLMPlanStrategy`.
2. Add `Foo` to the `LLMComplexItem::PlanMode` enum.
3. Add factory entry in `LLMComplexItem::_initStrategies()`.
4. Create `LLMFooEditor.qml` in `src/PlanView/`.
5. Register QML in `qgroundcontrol.qrc`.

No changes needed to the shell class or registration points.

## Voice AI Seam

The **VoicePlanStrategy** is a stub this round. The future integration entry point is:

```cpp
bool LLMComplexItem::applyMissionIntent(const QJsonObject& intent, QString& errorString);
```

Expected Mission-Intent JSON shape:
```jsonc
{
    "planMode": "spiral",         // or "fom"
    "origin": {
        "mode": "vehicle",       // "vehicle" | "mapClick" | "absolute"
        "lat": null,
        "lon": null
    },
    "params": {                   // keys match the chosen strategy's Facts
        "startRadius": 50,
        "spacing": 25,
        "turns": 5,
        "altitude": 80,
        "direction": 0
    }
}
```

The voice path will: capture audio → cloud multimodal LLM → parse response → call
`applyMissionIntent()` which sets planMode, populates facts, and calls recomputeGeometry —
reusing the exact same code path as the editor UI.

## Files

| File | Purpose |
|------|---------|
| `LLMPlanStrategy.h` | Abstract strategy interface |
| `LLMComplexItem.h/.cc` | Shell ComplexMissionItem |
| `SpiralPlanStrategy.h/.cc` | Archimedean spiral generator |
| `FomPlanStrategy.h/.cc` | FOM calibration circle generator |
| `VoicePlanStrategy.h/.cc` | Voice AI stub |
| `LLMPlanCreator.h/.cc` | PlanCreator for pattern menu |
| `README.md` | This file |
