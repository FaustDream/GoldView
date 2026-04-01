from __future__ import annotations

from typing import Optional

from PyQt6.QtCore import (
    QEasingCurve,
    QPoint,
    QPropertyAnimation,
    QRect,
    QSize,
    Qt,
    QTimer,
)
from PyQt6.QtGui import QCursor, QGuiApplication
from PyQt6.QtWidgets import QApplication, QLabel, QMainWindow, QVBoxLayout, QWidget

from core.settings_service import SettingsService


class FloatingWindowAdapter(QMainWindow):
    def __init__(self, settings_service: SettingsService) -> None:
        super().__init__()
        self._settings_service = settings_service
        self._dragging = False
        self._drag_offset = QPoint()
        self._edge_side: Optional[str] = None
        self._normal_geometry: Optional[QRect] = None
        self._is_hidden = False
        self._is_animating = False
        self._last_price_text = ""
        self._last_average_text = ""

        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Tool
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.setMinimumSize(QSize(118, 42))

        self._hide_timer = QTimer(self)
        self._hide_timer.setSingleShot(True)
        self._hide_timer.setInterval(1800)
        self._hide_timer.timeout.connect(self.hide_to_edge)

        self._mouse_watch_timer = QTimer(self)
        self._mouse_watch_timer.setInterval(200)
        self._mouse_watch_timer.timeout.connect(self.check_mouse_position)
        self._mouse_watch_timer.start()

        self._build_ui()

    def _build_ui(self) -> None:
        self._container = QWidget(self)
        self.setCentralWidget(self._container)
        self._container.setObjectName("FloatingContainer")

        layout = QVBoxLayout(self._container)
        layout.setContentsMargins(10, 6, 10, 6)
        layout.setSpacing(2)

        self._price_label = QLabel("--")
        self._price_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._price_label.setObjectName("PriceLabel")

        self._average_label = QLabel("")
        self._average_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._average_label.setObjectName("AverageLabel")

        layout.addWidget(self._price_label)
        layout.addWidget(self._average_label)

        self._container.setStyleSheet(
            """
            QWidget#FloatingContainer {
                background-color: rgba(255, 255, 255, 168);
                border: 1px solid rgba(255, 255, 255, 210);
                border-radius: 10px;
            }
            QLabel#PriceLabel {
                color: #101828;
                font-size: 18px;
                font-family: 'Segoe UI', 'Microsoft YaHei UI', sans-serif;
                font-weight: 700;
            }
            QLabel#AverageLabel {
                color: #344054;
                font-size: 12px;
                font-family: 'Segoe UI', 'Microsoft YaHei UI', sans-serif;
            }
            """
        )

    def apply_content(self, price_text: str, average_text: str) -> None:
        self._last_price_text = price_text
        self._last_average_text = average_text

        has_price = bool(price_text)
        has_average = bool(average_text)

        self._price_label.setVisible(has_price)
        self._average_label.setVisible(has_average)
        self._price_label.setText(price_text or "")
        self._average_label.setText(average_text or "")

        if has_price and has_average:
            self.resize(138, 58)
        else:
            self.resize(118, 42)

    def place_initially(self) -> None:
        saved_position = self._settings_service.get_floating_position()
        if saved_position is not None:
            self.move(saved_position)
            return

        screens = QGuiApplication.screens()
        target_screen = max(screens, key=lambda screen: screen.geometry().right())
        available = target_screen.availableGeometry()
        self.move(available.right() - self.width() - 12, available.top() + 96)

    def mousePressEvent(self, event) -> None:  # type: ignore[override]
        if event.button() == Qt.MouseButton.LeftButton:
            self._dragging = True
            self._drag_offset = event.globalPosition().toPoint() - self.frameGeometry().topLeft()
            self._hide_timer.stop()
            event.accept()

    def mouseMoveEvent(self, event) -> None:  # type: ignore[override]
        if self._dragging:
            target = event.globalPosition().toPoint() - self._drag_offset
            screen = QGuiApplication.screenAt(event.globalPosition().toPoint()) or QApplication.primaryScreen()
            available = screen.availableGeometry()
            x = max(available.left(), min(target.x(), available.right() - self.width()))
            y = max(available.top(), min(target.y(), available.bottom() - self.height()))
            self.move(x, y)
            event.accept()

    def mouseReleaseEvent(self, event) -> None:  # type: ignore[override]
        self._dragging = False
        self._settings_service.set_floating_position(self.pos())
        self.check_edge_status()
        super().mouseReleaseEvent(event)

    def check_edge_status(self) -> None:
        screen = QGuiApplication.screenAt(self.geometry().center()) or QApplication.primaryScreen()
        available = screen.availableGeometry()
        rect = self.geometry()
        snap_threshold = 16
        self._edge_side = None

        if abs(rect.left() - available.left()) <= snap_threshold:
            self._edge_side = "left"
            self.move(available.left(), rect.top())
        elif abs(rect.right() - available.right()) <= snap_threshold:
            self._edge_side = "right"
            self.move(available.right() - rect.width(), rect.top())
        elif abs(rect.top() - available.top()) <= snap_threshold:
            self._edge_side = "top"
            self.move(rect.left(), available.top())

        if self._edge_side and not self._is_hidden:
            self._normal_geometry = self.geometry()
            self._hide_timer.start()
        else:
            self._hide_timer.stop()

    def hide_to_edge(self) -> None:
        if not self._edge_side or self._is_hidden or self._dragging:
            return

        screen = QGuiApplication.screenAt(self.geometry().center()) or QApplication.primaryScreen()
        available = screen.availableGeometry()
        rect = self.geometry()
        offset = 6

        if self._edge_side == "left":
            target = QRect(available.left() - rect.width() + offset, rect.top(), rect.width(), rect.height())
        elif self._edge_side == "right":
            target = QRect(available.right() - offset, rect.top(), rect.width(), rect.height())
        else:
            target = QRect(rect.left(), available.top() - rect.height() + offset, rect.width(), rect.height())

        self._animate_to(target)
        self._is_hidden = True

    def show_normal(self) -> None:
        if self._is_hidden and self._normal_geometry is not None:
            self._animate_to(self._normal_geometry)
            self._is_hidden = False

    def check_mouse_position(self) -> None:
        if self._dragging or self._is_animating:
            return

        cursor_pos = QCursor.pos()
        sense_rect = self.geometry().adjusted(-8, -8, 8, 8)
        if sense_rect.contains(cursor_pos):
            self._hide_timer.stop()
            self.show_normal()
        else:
            if not self._is_hidden:
                self.check_edge_status()

    def _animate_to(self, rect: QRect) -> None:
        self._is_animating = True
        self._animation = QPropertyAnimation(self, b"geometry")
        self._animation.setDuration(180)
        self._animation.setStartValue(self.geometry())
        self._animation.setEndValue(rect)
        self._animation.setEasingCurve(QEasingCurve.Type.OutCubic)
        self._animation.finished.connect(self._on_animation_finished)
        self._animation.start()

    def _on_animation_finished(self) -> None:
        self._is_animating = False
        if not self._is_hidden:
            self._settings_service.set_floating_position(self.pos())
