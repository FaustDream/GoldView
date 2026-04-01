from __future__ import annotations

import sys
from typing import Dict, Optional

from PyQt6.QtCore import QSize, Qt
from PyQt6.QtGui import QAction, QActionGroup, QIcon, QPainter, QPixmap
from PyQt6.QtSvg import QSvgRenderer
from PyQt6.QtWidgets import QApplication, QMenu, QSystemTrayIcon

from calculator import GoldCalculatorWindow
from core.average_service import AverageService
from core.price_service import PriceService
from core.settings_service import SettingsService
from shell.mode_manager import ModeManager


def create_tray_icon() -> QIcon:
    svg_content = """
    <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
      <path d="M10,80 L25,30 L75,30 L90,80 Z" fill="#90EE90" stroke="#2E8B57" stroke-width="3"/>
      <path d="M25,30 L35,20 L65,20 L75,30 Z" fill="#C1FFC1" stroke="#2E8B57" stroke-width="2"/>
      <line x1="30" y1="45" x2="70" y2="45" stroke="#2E8B57" stroke-width="2" stroke-linecap="round"/>
      <line x1="25" y1="60" x2="75" y2="60" stroke="#2E8B57" stroke-width="2" stroke-linecap="round"/>
    </svg>
    """
    renderer = QSvgRenderer(svg_content.encode("utf-8"))
    pixmap = QPixmap(QSize(64, 64))
    pixmap.fill(Qt.GlobalColor.transparent)
    painter = QPainter(pixmap)
    renderer.render(painter)
    painter.end()
    return QIcon(pixmap)


class GoldViewApplication:
    def __init__(self, app: QApplication) -> None:
        self._app = app
        self._app.setQuitOnLastWindowClosed(False)
        self._app.setApplicationName("GoldView")

        self._settings_service = SettingsService()
        self._average_service = AverageService()
        self._price_service = PriceService(self._settings_service)
        self._mode_manager = ModeManager(self._settings_service, self._price_service)
        self._calculator_window: Optional[GoldCalculatorWindow] = None

        self._tray_icon = QSystemTrayIcon(create_tray_icon(), self._app)
        self._tray_menu = QMenu()
        self._mode_action_group = QActionGroup(self._tray_menu)
        self._mode_action_group.setExclusive(True)

        self._build_tray_menu()
        self._mode_manager.start()
        self._tray_icon.show()

    def _build_tray_menu(self) -> None:
        calculator_action = QAction("均价计算器", self._tray_menu)
        calculator_action.triggered.connect(lambda _checked=False: self.open_calculator())
        self._tray_menu.addAction(calculator_action)
        self._tray_menu.addSeparator()

        mode_menu = self._tray_menu.addMenu("显示模式")

        floating_action = QAction("悬浮模式", mode_menu)
        floating_action.setCheckable(True)
        floating_action.setData(SettingsService.MODE_FLOATING)
        floating_action.triggered.connect(
            lambda _checked=False: self._settings_service.set_display_mode(SettingsService.MODE_FLOATING)
        )
        self._mode_action_group.addAction(floating_action)
        mode_menu.addAction(floating_action)

        taskbar_action = QAction("任务栏窗口模式", mode_menu)
        taskbar_action.setCheckable(True)
        taskbar_action.setData(SettingsService.MODE_TASKBAR)
        taskbar_action.triggered.connect(
            lambda _checked=False: self._settings_service.set_display_mode(SettingsService.MODE_TASKBAR)
        )
        self._mode_action_group.addAction(taskbar_action)
        mode_menu.addAction(taskbar_action)

        self._show_gold_price_action = QAction("显示黄金价格", self._tray_menu)
        self._show_gold_price_action.setCheckable(True)
        self._show_gold_price_action.triggered.connect(
            lambda checked: self._settings_service.set_show_gold_price(checked)
        )
        self._tray_menu.addAction(self._show_gold_price_action)

        self._show_average_info_action = QAction("显示均价信息", self._tray_menu)
        self._show_average_info_action.setCheckable(True)
        self._show_average_info_action.triggered.connect(
            lambda checked: self._settings_service.set_show_average_info(checked)
        )
        self._tray_menu.addAction(self._show_average_info_action)

        refresh_menu = self._tray_menu.addMenu("刷新频率")
        self._refresh_actions: Dict[float, QAction] = {}
        for seconds, label in ((0.5, "0.5 秒"), (1.0, "1 秒"), (2.0, "2 秒")):
            action = QAction(label, refresh_menu)
            action.setCheckable(True)
            action.triggered.connect(
                lambda checked, interval=seconds: checked and self._settings_service.set_refresh_interval(interval)
            )
            refresh_menu.addAction(action)
            self._refresh_actions[seconds] = action

        self._tray_menu.addSeparator()
        quit_action = QAction("退出程序", self._tray_menu)
        quit_action.triggered.connect(lambda _checked=False: self.quit())
        self._tray_menu.addAction(quit_action)

        self._tray_icon.setContextMenu(self._tray_menu)
        self._settings_service.changed.connect(self._sync_actions)
        self._sync_actions()

    def _sync_actions(self, *_args) -> None:
        mode = self._settings_service.get_display_mode()
        for action in self._mode_action_group.actions():
            action.setChecked(action.data() == mode)

        self._show_gold_price_action.setChecked(self._settings_service.get_show_gold_price())
        self._show_average_info_action.setChecked(self._settings_service.get_show_average_info())

        refresh_interval = self._settings_service.get_refresh_interval()
        for interval, action in self._refresh_actions.items():
            action.setChecked(interval == refresh_interval)

    def open_calculator(self) -> None:
        if self._calculator_window is None:
            self._calculator_window = GoldCalculatorWindow(
                settings_service=self._settings_service,
                average_service=self._average_service,
            )
        self._calculator_window.show()
        self._calculator_window.raise_()
        self._calculator_window.activateWindow()

    def quit(self) -> None:
        self._mode_manager.shutdown()
        self._tray_icon.hide()
        self._app.quit()


def main() -> int:
    app = QApplication(sys.argv)
    GoldViewApplication(app)
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
