#pragma once

#include "taskbar_topology_detector.h"

namespace goldview {

class TaskbarTrayDetector {
public:
    void enrich(TaskbarTopology& topology) const;
};

}  // namespace goldview
