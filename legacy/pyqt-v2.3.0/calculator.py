from __future__ import annotations

import sys
from typing import List, Optional, Tuple

from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QColor, QCursor, QDoubleValidator
from PyQt6.QtWidgets import (
    QApplication,
    QAbstractItemView,
    QFrame,
    QGraphicsDropShadowEffect,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

from core.average_service import AverageService
from core.settings_service import SettingsService


class ClickableLabel(QLabel):
    clicked = pyqtSignal()

    def __init__(self, text: str, parent: QWidget | None = None) -> None:
        super().__init__(text, parent)
        self.setCursor(QCursor(Qt.CursorShape.PointingHandCursor))

    def mousePressEvent(self, event) -> None:  # type: ignore[override]
        if event.button() == Qt.MouseButton.LeftButton:
            self.clicked.emit()
        super().mousePressEvent(event)


class GoldCalculatorWindow(QWidget):
    def __init__(
        self,
        settings_service: Optional[SettingsService] = None,
        average_service: Optional[AverageService] = None,
    ) -> None:
        super().__init__()
        self._settings_service = settings_service or SettingsService()
        self._average_service = average_service or AverageService()
        self._history_visible = False
        self._height_hidden = 580
        self._height_shown = 720

        self.setWindowTitle("均价计算器")
        self.setWindowFlags(Qt.WindowType.WindowStaysOnTopHint)
        self.setFixedWidth(360)
        self.setFixedHeight(self._height_hidden)

        self._build_ui()
        self._load_history()

    def _build_ui(self) -> None:
        self.setStyleSheet(
            """
            QWidget {
                background-color: #F5F7FA;
            }
            QLabel#TitleLabel {
                font-family: 'Segoe UI', 'Microsoft YaHei UI';
                font-size: 18px;
                font-weight: 600;
                color: #1F2937;
            }
            QFrame[class="Card"] {
                background-color: #FFFFFF;
                border-radius: 12px;
            }
            QLabel[class="Label"] {
                font-family: 'Segoe UI', 'Microsoft YaHei UI';
                font-size: 13px;
                color: #6B7280;
            }
            QLineEdit {
                height: 40px;
                border-radius: 8px;
                border: 1px solid #E5E7EB;
                padding: 0 12px;
                font-family: 'Segoe UI', 'Microsoft YaHei UI';
                font-size: 14px;
                color: #111827;
                background-color: #FFFFFF;
            }
            QLineEdit:focus {
                border: 1px solid #3B82F6;
            }
            QPushButton {
                height: 44px;
                border-radius: 10px;
                font-family: 'Segoe UI', 'Microsoft YaHei UI';
                font-size: 14px;
                font-weight: 500;
            }
            QPushButton#CalcButton {
                background-color: #3B82F6;
                color: #FFFFFF;
            }
            QPushButton#CalcButton:hover {
                background-color: #2563EB;
            }
            QPushButton#ClearButton {
                background-color: #FFFFFF;
                border: 1px solid #E5E7EB;
                color: #4B5563;
            }
            QPushButton#ClearButton:hover {
                background-color: #F9FAFB;
            }
            QFrame#ResultCard {
                border-radius: 12px;
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                    stop:0 #3B82F6, stop:1 #6366F1);
            }
            QLabel#ResultPrice {
                font-family: 'Segoe UI', 'Microsoft YaHei UI';
                font-size: 24px;
                font-weight: 700;
                color: #FFFFFF;
            }
            QLabel#ResultSub {
                font-family: 'Segoe UI', 'Microsoft YaHei UI';
                font-size: 13px;
                color: rgba(255, 255, 255, 0.85);
            }
            QListWidget {
                background-color: transparent;
                border: none;
                outline: none;
            }
            QListWidget::item {
                background-color: #FFFFFF;
                border-radius: 8px;
                margin-bottom: 8px;
                padding: 10px;
                color: #4B5563;
                font-size: 12px;
            }
            QLabel#HistoryToggleLabel {
                font-family: 'Segoe UI', 'Microsoft YaHei UI';
                font-size: 13px;
                font-weight: 600;
                color: #6B7280;
                margin-top: 10px;
            }
            QLabel#HistoryToggleLabel:hover {
                color: #3B82F6;
            }
            """
        )

        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(24, 24, 24, 24)
        main_layout.setSpacing(20)

        title = QLabel("均价计算器")
        title.setObjectName("TitleLabel")
        main_layout.addWidget(title)

        input_card = QFrame()
        input_card.setProperty("class", "Card")
        self._add_shadow(input_card)
        input_layout = QVBoxLayout(input_card)
        input_layout.setContentsMargins(16, 16, 16, 16)
        input_layout.setSpacing(16)

        self._validator = QDoubleValidator(0.0, 999999.0, 4)
        self._validator.setNotation(QDoubleValidator.Notation.StandardNotation)

        self.old_price_input, self.old_weight_input = self._create_form_row(
            input_layout,
            "原持仓",
            "价格 (元/克)",
            "重量 (克)",
        )
        self.new_price_input, self.new_weight_input = self._create_form_row(
            input_layout,
            "新买入",
            "价格 (元/克)",
            "重量 (克)",
        )

        main_layout.addWidget(input_card)

        button_layout = QHBoxLayout()
        button_layout.setSpacing(12)

        clear_button = QPushButton("清空")
        clear_button.setObjectName("ClearButton")
        clear_button.clicked.connect(self.clear_inputs)
        button_layout.addWidget(clear_button, 1)

        calculate_button = QPushButton("开始计算")
        calculate_button.setObjectName("CalcButton")
        calculate_button.clicked.connect(self.calculate)
        button_layout.addWidget(calculate_button, 2)

        main_layout.addLayout(button_layout)

        result_card = QFrame()
        result_card.setObjectName("ResultCard")
        self._add_shadow(result_card, blur=12, opacity=30)
        result_layout = QVBoxLayout(result_card)
        result_layout.setContentsMargins(20, 20, 20, 20)
        result_layout.setSpacing(6)

        self.result_price_label = QLabel("均价: --")
        self.result_price_label.setObjectName("ResultPrice")
        self.result_price_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        self.result_total_label = QLabel("总持有量: -- 克")
        self.result_total_label.setObjectName("ResultSub")
        self.result_total_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        result_layout.addWidget(self.result_price_label)
        result_layout.addWidget(self.result_total_label)
        main_layout.addWidget(result_card)

        self.history_toggle = ClickableLabel("最近记录 ▼")
        self.history_toggle.setObjectName("HistoryToggleLabel")
        self.history_toggle.clicked.connect(self.toggle_history)
        main_layout.addWidget(self.history_toggle)

        self.history_list = QListWidget()
        self.history_list.setSelectionMode(QAbstractItemView.SelectionMode.NoSelection)
        self.history_list.setFixedHeight(120)
        self.history_list.setVisible(False)
        main_layout.addWidget(self.history_list)

        main_layout.addStretch()

    def _create_form_row(
        self,
        parent_layout: QVBoxLayout,
        group_title: str,
        first_label: str,
        second_label: str,
    ) -> Tuple[QLineEdit, QLineEdit]:
        title = QLabel(group_title)
        title.setProperty("class", "Label")
        title.setStyleSheet("font-weight: 600; color: #374151; margin-bottom: 2px;")
        parent_layout.addWidget(title)

        row_layout = QHBoxLayout()
        row_layout.setSpacing(12)

        first_box = QVBoxLayout()
        first_box.setSpacing(6)
        first_label_widget = QLabel(first_label)
        first_label_widget.setProperty("class", "Label")
        first_input = QLineEdit()
        first_input.setPlaceholderText("0.00")
        first_input.setValidator(self._validator)
        first_box.addWidget(first_label_widget)
        first_box.addWidget(first_input)

        second_box = QVBoxLayout()
        second_box.setSpacing(6)
        second_label_widget = QLabel(second_label)
        second_label_widget.setProperty("class", "Label")
        second_input = QLineEdit()
        second_input.setPlaceholderText("0.00")
        second_input.setValidator(self._validator)
        second_box.addWidget(second_label_widget)
        second_box.addWidget(second_input)

        row_layout.addLayout(first_box)
        row_layout.addLayout(second_box)
        parent_layout.addLayout(row_layout)
        return first_input, second_input

    def _add_shadow(self, widget: QWidget, blur: int = 12, opacity: int = 13) -> None:
        shadow = QGraphicsDropShadowEffect()
        shadow.setBlurRadius(blur)
        shadow.setColor(QColor(0, 0, 0, opacity))
        shadow.setOffset(0, 4)
        widget.setGraphicsEffect(shadow)

    def toggle_history(self) -> None:
        self._history_visible = not self._history_visible
        self.history_list.setVisible(self._history_visible)
        self.history_toggle.setText("最近记录 ▲" if self._history_visible else "最近记录 ▼")
        self.setFixedHeight(self._height_shown if self._history_visible else self._height_hidden)

    def clear_inputs(self) -> None:
        for widget in (
            self.old_price_input,
            self.old_weight_input,
            self.new_price_input,
            self.new_weight_input,
        ):
            widget.clear()
        self.result_price_label.setText("均价: --")
        self.result_total_label.setText("总持有量: -- 克")

    def calculate(self) -> None:
        values = [
            self.old_price_input.text().strip(),
            self.old_weight_input.text().strip(),
            self.new_price_input.text().strip(),
            self.new_weight_input.text().strip(),
        ]
        if not all(values):
            self.result_price_label.setText("请输入数据")
            self.result_total_label.setText("四个输入框均不能为空")
            return

        try:
            calculation = self._average_service.calculate(*(float(value) for value in values))
        except ValueError:
            self.result_price_label.setText("输入非法")
            self.result_total_label.setText("请输入有效的数字格式")
            return

        self.result_price_label.setText(f"均价: {calculation.average_price:.2f}")
        self.result_total_label.setText(f"总持有量: {calculation.total_weight:.2f} 克")

        summary = calculation.summary_text
        history_record = (
            f"{float(values[0]):.2f}/{float(values[1]):.2f} + "
            f"{float(values[2]):.2f}/{float(values[3]):.2f} -> {calculation.average_price:.2f}"
        )

        self._settings_service.set_last_average_summary(summary)
        self._save_history(history_record)

    def _save_history(self, record_text: str) -> None:
        history = self._settings_service.get_history()
        history.insert(0, record_text)
        history = history[:5]
        self._settings_service.set_history(history)
        self._render_history(history)

    def _load_history(self) -> None:
        self._render_history(self._settings_service.get_history())

    def _render_history(self, history: List[str]) -> None:
        self.history_list.clear()
        for item in history:
            self.history_list.addItem(item)


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = GoldCalculatorWindow()
    window.show()
    sys.exit(app.exec())
