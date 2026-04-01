from __future__ import annotations

import re
import threading
import time

import requests
from PyQt6.QtCore import QObject, QThread, pyqtSignal

from core.models import PriceSnapshot
from core.settings_service import SettingsService


class PricePollingThread(QThread):
    snapshot_ready = pyqtSignal(object)

    def __init__(self, interval_seconds: float = 1.0) -> None:
        super().__init__()
        self._interval_seconds = interval_seconds
        self._interval_lock = threading.Lock()
        self._stop_event = threading.Event()

    def set_interval(self, interval_seconds: float) -> None:
        with self._interval_lock:
            self._interval_seconds = max(0.5, interval_seconds)

    def stop(self) -> None:
        self._stop_event.set()

    def run(self) -> None:
        while not self._stop_event.is_set():
            snapshot = self.fetch_snapshot()
            if snapshot is not None:
                self.snapshot_ready.emit(snapshot)

            with self._interval_lock:
                interval = self._interval_seconds
            self._stop_event.wait(interval)

    def fetch_snapshot(self) -> PriceSnapshot | None:
        try:
            url = "https://hq.sinajs.cn/list=hf_XAU"
            headers = {"Referer": "https://finance.sina.com.cn"}
            response = requests.get(url, headers=headers, timeout=2)
            content = response.content.decode("gbk", errors="ignore")
            match = re.search(r'"(.*)"', content)
            if match:
                data = match.group(1).split(",")
                if len(data) > 10:
                    current_price = float(data[0])
                    low_price = float(data[5])
                    open_price = float(data[8])
                    return PriceSnapshot(
                        current_price=current_price,
                        open_price=open_price,
                        low_price=low_price,
                        source="sina",
                    )
        except Exception:
            pass

        try:
            response = requests.get("https://api.gold-api.com/v1/gold", timeout=2)
            if response.status_code == 200:
                data = response.json()
                current_price = float(data.get("price", 0.0))
                return PriceSnapshot(
                    current_price=current_price,
                    open_price=current_price,
                    low_price=current_price,
                    source="gold-api",
                )
        except Exception:
            pass

        return None


class PriceService(QObject):
    snapshot_updated = pyqtSignal(object)

    def __init__(self, settings_service: SettingsService) -> None:
        super().__init__()
        self._settings_service = settings_service
        self._thread = PricePollingThread(settings_service.get_refresh_interval())
        self._thread.snapshot_ready.connect(self.snapshot_updated.emit)

    def start(self) -> None:
        if not self._thread.isRunning():
            self._thread.start()

    def stop(self) -> None:
        if self._thread.isRunning():
            self._thread.stop()
            self._thread.wait(3000)

    def set_interval(self, interval_seconds: float) -> None:
        self._thread.set_interval(interval_seconds)
