from __future__ import annotations

from PyQt6.QtCore import QSize, Qt
from PyQt6.QtGui import QGuiApplication
from PyQt6.QtWidgets import QLabel, QMainWindow, QVBoxLayout, QWidget


class TaskbarWindowAdapter(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Tool
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.setFixedSize(QSize(220, 58))
        self._build_ui()

    def _build_ui(self) -> None:
        container = QWidget(self)
        container.setObjectName("TaskbarContainer")
        self.setCentralWidget(container)

        layout = QVBoxLayout(container)
        layout.setContentsMargins(12, 8, 12, 8)
        layout.setSpacing(1)

        self._price_label = QLabel("--")
        self._price_label.setObjectName("TaskbarPrice")
        self._price_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        self._average_label = QLabel("")
        self._average_label.setObjectName("TaskbarAverage")
        self._average_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        layout.addWidget(self._price_label)
        layout.addWidget(self._average_label)

        container.setStyleSheet(
            """
            QWidget#TaskbarContainer {
                background-color: rgba(18, 22, 30, 230);
                border: 1px solid rgba(75, 85, 99, 180);
                border-radius: 10px;
            }
            QLabel#TaskbarPrice {
                color: #86ff9a;
                font-size: 22px;
                font-family: Consolas, 'Segoe UI', monospace;
                font-weight: 700;
            }
            QLabel#TaskbarAverage {
                color: #9ad7ff;
                font-size: 14px;
                font-family: 'Segoe UI', 'Microsoft YaHei UI', sans-serif;
            }
            """
        )

    def apply_content(self, price_text: str, average_text: str) -> None:
        self._price_label.setVisible(bool(price_text))
        self._average_label.setVisible(bool(average_text))
        self._price_label.setText(price_text or "")
        self._average_label.setText(average_text or "")
        self.reposition()

    def showEvent(self, event) -> None:  # type: ignore[override]
        self.reposition()
        super().showEvent(event)

    def reposition(self) -> None:
        screen = QGuiApplication.primaryScreen()
        available = screen.availableGeometry()
        x = available.right() - self.width() - 16
        y = available.bottom() - self.height() - 10
        self.move(x, y)
