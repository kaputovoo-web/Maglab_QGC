#pragma once

#include "LLMComplexItem.h"
#include "QmlObjectListModel.h"

namespace MaglabMissionValidation {

inline bool validate(QmlObjectListModel* items)
{
    if (!items) {
        return false;
    }
    for (int i = 0; i < items->count(); ++i) {
        const auto* llm = qobject_cast<LLMComplexItem*>(items->get(i));
        if (llm && !llm->validateMissionOutput()) {
            return false;
        }
    }
    return true;
}

} // namespace MaglabMissionValidation
