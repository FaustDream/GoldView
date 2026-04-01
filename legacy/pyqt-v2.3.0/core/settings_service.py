from __future__ import annotations

from typing import Iterable, List, Optional

from PyQt6.QtCore import QObject, QPoint, QSettings, pyqtSignal


class SettingsService(QObject):
    MODE_FLOATING = "floating-window"
    MODE_TASKBAR = "taskbar-window"
    VALID_MODES = {MODE_FLOATING, MODE_TASKBAR}

    changed = pyqtSignal(str, object)

    def __init__(self) -> None:
        super().__init__()
        self._settings = QSettings("GoldView", "GoldView")
        self._legacy_calculator_settings = QSettings("GoldView", "Calculator")

    def _set_value(self, key: str, value: object) -> None:
        if self._settings.value(key) == value:
            return
        self._settings.setValue(key, value)
        self.changed.emit(key, value)

    def get_display_mode(self) -> str:
        mode = self._settings.value("display/mode", self.MODE_FLOATING, str)
        if mode not in self.VALID_MODES:
            return self.MODE_FLOATING
        return mode

    def set_display_mode(self, mode: str) -> None:
        if mode not in self.VALID_MODES:
            raise ValueError(f"unsupported mode: {mode}")
        self._set_value("display/mode", mode)

    def get_show_gold_price(self) -> bool:
        return self._settings.value("display/show_gold_price", True, bool)

    def set_show_gold_price(self, enabled: bool) -> None:
        self._set_value("display/show_gold_price", bool(enabled))

    def get_show_average_info(self) -> bool:
        return self._settings.value("display/show_average_info", True, bool)

    def set_show_average_info(self, enabled: bool) -> None:
        self._set_value("display/show_average_info", bool(enabled))

    def get_refresh_interval(self) -> float:
        value = self._settings.value("display/refresh_interval", 1.0, float)
        return max(0.5, float(value))

    def set_refresh_interval(self, seconds: float) -> None:
        self._set_value("display/refresh_interval", max(0.5, float(seconds)))

    def get_last_average_summary(self) -> str:
        return self._settings.value("calculator/last_average_summary", "", str)

    def set_last_average_summary(self, summary: str) -> None:
        self._set_value("calculator/last_average_summary", summary)

    def get_history(self) -> List[str]:
        history = self._settings.value("calculator/history")
        if history is None:
            history = self._legacy_calculator_settings.value("history", [])
        if history is None:
            return []
        if isinstance(history, str):
            return [history]
        if isinstance(history, Iterable):
            return [str(item) for item in history]
        return []

    def set_history(self, history: List[str]) -> None:
        self._set_value("calculator/history", history)

    def get_floating_position(self) -> Optional[QPoint]:
        value = self._settings.value("floating/position")
        if isinstance(value, QPoint):
            return value
        return None

    def set_floating_position(self, point: QPoint) -> None:
        self._set_value("floating/position", point)
