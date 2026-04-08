#pragma once

#include <string>

namespace goldview {

struct AverageCalculation {
    double averagePrice{0.0};
    double totalWeight{0.0};
};

class AverageService {
public:
    AverageCalculation calculate(double oldPrice, double oldWeight, double newPrice, double newWeight) const;
    std::wstring formatHistoryRecord(double oldPrice, double oldWeight, double newPrice, double newWeight,
                                     const AverageCalculation& calculation) const;
};

}  // namespace goldview
