from __future__ import annotations

from dataclasses import dataclass, field
from time import time


@dataclass
class PriceSnapshot:
    current_price: float
    open_price: float
    low_price: float
    source: str
    updated_at: float = field(default_factory=time)


@dataclass
class AverageCalculation:
    average_price: float
    total_weight: float

    @property
    def summary_text(self) -> str:
        return f"均价 {self.average_price:.2f}"
