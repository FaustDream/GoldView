from __future__ import annotations

from core.models import AverageCalculation


class AverageService:
    """Provides shared average-price calculations for the calculator and display modes."""

    def calculate(
        self,
        old_price: float,
        old_weight: float,
        new_price: float,
        new_weight: float,
    ) -> AverageCalculation:
        total_weight = old_weight + new_weight
        if total_weight <= 0:
            raise ValueError("total weight must be greater than zero")

        total_cost = (old_price * old_weight) + (new_price * new_weight)
        average_price = total_cost / total_weight
        return AverageCalculation(average_price=average_price, total_weight=total_weight)
