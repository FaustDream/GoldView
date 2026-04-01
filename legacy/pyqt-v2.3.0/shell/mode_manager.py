from __future__ import annotations

from typing import Optional

from PyQt6.QtCore import QObject, pyqtSignal

from core.models import PriceSnapshot
from core.price_service import PriceService
from core.settings_service import SettingsService
from shell.floating_window_adapter import FloatingWindowAdapter
from shell.taskbar_window_adapter import TaskbarWindowAdapter


class ModeManager(QObject):
    calculator_requested = pyqtSignal()

    def __init__(self, settings_service: SettingsService, price_service: PriceService) -> None:
        super().__init__()
        self._settings_service = settings_service
        self._price_service = price_service
        self._current_view = None
        self._last_snapshot: Optional[PriceSnapshot] = None

        self._price_service.snapshot_updated.connect(self._on_snapshot_updated)
        self._settings_service.changed.connect(self._on_settings_changed)

    def start(self) -> None:
        self._switch_to_mode(self._settings_service.get_display_mode())
        self._price_service.start()

    def shutdown(self) -> None:
        self._price_service.stop()
        if self._current_view is not None:
            self._current_view.close()
            self._current_view = None

    def _on_snapshot_updated(self, snapshot: PriceSnapshot) -> None:
        self._last_snapshot = snapshot
        self._render()

    def _on_settings_changed(self, key: str, value: object) -> None:
        if key == "display/mode":
            self._switch_to_mode(str(value))
            return
        if key == "display/refresh_interval":
            self._price_service.set_interval(float(value))
        self._render()

    def _switch_to_mode(self, mode: str) -> None:
        existing_snapshot = self._last_snapshot
        if self._current_view is not None:
            self._current_view.close()
            self._current_view = None

        if mode == SettingsService.MODE_TASKBAR:
            self._current_view = TaskbarWindowAdapter()
        else:
            floating = FloatingWindowAdapter(self._settings_service)
            floating.place_initially()
            self._current_view = floating

        self._current_view.show()
        self._current_view.raise_()
        if existing_snapshot is not None:
            self._render()

    def _render(self) -> None:
        if self._current_view is None:
            return

        price_text = ""
        average_text = ""

        if self._settings_service.get_show_gold_price():
            if self._last_snapshot is None:
                price_text = "--.--"
            else:
                price_text = f"{self._last_snapshot.current_price:.2f}"

        if self._settings_service.get_show_average_info():
            average_text = self._settings_service.get_last_average_summary()

        self._current_view.apply_content(price_text, average_text)
