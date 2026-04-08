#pragma once

#include "models.h"
#include "taskbar_metrics.h"
#include "taskbar_topology_detector.h"

namespace goldview {

class TaskbarAnchorResolver {
public:
    TaskbarAnchor resolve(const TaskbarTopology& topology, const TaskbarLayoutMetrics& metrics) const;
};

}  // namespace goldview
