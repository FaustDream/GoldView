#pragma once

#include "models.h"
#include "taskbar_topology_detector.h"

namespace goldview {

struct TaskbarLayoutMetrics {
    bool available{true};
    int desiredWidth{176};
    int desiredHeight{28};
    int horizontalPadding{6};
    int verticalPadding{4};
    bool prefersTaskbarReflow{true};
    std::wstring reason;
};

class TaskbarMetrics {
public:
    TaskbarLayoutMetrics calculate(const TaskbarTopology& topology, const DisplaySettings& settings) const;
};

}  // namespace goldview
