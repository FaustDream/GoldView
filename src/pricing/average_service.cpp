#include "average_service.h"

#include <stdexcept>

namespace goldview {

AverageCalculation AverageService::calculate(
    double oldPrice,
    double oldWeight,
    double newPrice,
    double newWeight) const {
    const double totalWeight = oldWeight + newWeight;
    if (totalWeight <= 0.0) {
        throw std::invalid_argument("total weight must be greater than zero");
    }

    const double totalCost = (oldPrice * oldWeight) + (newPrice * newWeight);
    return AverageCalculation{totalCost / totalWeight, totalWeight};
}

std::wstring AverageService::formatHistoryRecord(
    double oldPrice,
    double oldWeight,
    double newPrice,
    double newWeight,
    const AverageCalculation& calculation) const {
    wchar_t buffer[160]{};
    swprintf_s(
        buffer,
        L"%.2f/%.2fg + %.2f/%.2fg -> %.2f",
        oldPrice,
        oldWeight,
        newPrice,
        newWeight,
        calculation.averagePrice);
    return buffer;
}

}  // namespace goldview
