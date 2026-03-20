import sys
import json
from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, 
                             QLineEdit, QPushButton, QFrame, QGraphicsDropShadowEffect,
                             QListWidget, QListWidgetItem, QAbstractItemView)
from PyQt6.QtCore import Qt, QSettings, pyqtSignal
from PyQt6.QtGui import QColor, QDoubleValidator, QCursor

class ClickableLabel(QLabel):
    clicked = pyqtSignal()
    def __init__(self, text, parent=None):
        super().__init__(text, parent)
        self.setCursor(QCursor(Qt.CursorShape.PointingHandCursor))
    
    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            self.clicked.emit()
        super().mousePressEvent(event)

class GoldCalculatorWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("均价计算器")
        self.setWindowFlags(Qt.WindowType.WindowStaysOnTopHint)
        self.settings = QSettings("GoldView", "Calculator")
        
        # 初始高度（隐藏历史记录时）
        self.height_hidden = 580
        self.height_shown = 720
        self.setFixedWidth(360)
        self.setFixedHeight(self.height_hidden)
        
        self.init_ui()
        self.load_history()
        
        # 默认隐藏历史记录
        self.history_visible = False
        self.history_list.setVisible(False)

    def init_ui(self):
        # 1. 整体风格设定 (基于用户提供的 CSS 规范)
        self.setStyleSheet("""
            GoldCalculatorWindow {
                background-color: #F5F7FA;
            }
            
            /* 标题栏 */
            #TitleLabel {
                font-family: 'Segoe UI', 'Microsoft YaHei UI';
                font-size: 18px;
                font-weight: 600;
                color: #1F2937;
            }

            /* 卡片容器 */
            .Card {
                background-color: #FFFFFF;
                border-radius: 12px;
            }

            /* Label */
            .Label {
                font-family: 'Segoe UI', 'Microsoft YaHei UI';
                font-size: 13px;
                font-weight: 400;
                color: #6B7280;
            }

            /* 输入框 */
            QLineEdit {
                height: 40px;
                border-radius: 8px;
                border: 1px solid #E5E7EB;
                padding: 0 12px;
                font-family: 'Segoe UI', 'Microsoft YaHei UI';
                font-size: 14px;
                font-weight: 500;
                color: #111827;
                background-color: #FFFFFF;
            }
            QLineEdit:focus {
                border: 1px solid #3B82F6;
                background-color: #FFFFFF;
            }

            /* 按钮通用 */
            QPushButton {
                height: 44px;
                border-radius: 10px;
                font-family: 'Segoe UI', 'Microsoft YaHei UI';
                font-size: 14px;
                font-weight: 500;
            }

            /* 计算按钮 */
            #CalcButton {
                background-color: #3B82F6;
                color: #FFFFFF;
            }
            #CalcButton:hover {
                background-color: #2563EB;
            }
            #CalcButton:pressed {
                background-color: #1D4ED8;
            }

            /* 清空按钮 */
            #ClearButton {
                background-color: #FFFFFF;
                border: 1px solid #E5E7EB;
                color: #4B5563;
            }
            #ClearButton:hover {
                background-color: #F9FAFB;
                border-color: #D1D5DB;
            }
            #ClearButton:pressed {
                background-color: #F3F4F6;
            }

            /* 结果卡片 */
            #ResultCard {
                border-radius: 12px;
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1, 
                    stop:0 #3B82F6, stop:1 #6366F1);
            }
            #ResultPrice {
                font-family: 'Segoe UI', 'Microsoft YaHei UI';
                font-size: 24px;
                font-weight: 700;
                color: #FFFFFF;
            }
            #ResultSub {
                font-family: 'Segoe UI', 'Microsoft YaHei UI';
                font-size: 13px;
                font-weight: 400;
                color: rgba(255, 255, 255, 0.85);
            }
            
            /* 历史记录列表 */
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
            QListWidget::item:selected {
                background-color: #EFF6FF;
                color: #1D4ED8;
            }
            
            /* 可点击标题 */
            #HistoryToggleLabel {
                font-family: 'Segoe UI', 'Microsoft YaHei UI';
                font-size: 13px;
                font-weight: 600;
                color: #6B7280;
                margin-top: 10px;
            }
            #HistoryToggleLabel:hover {
                color: #3B82F6;
            }
        """)

        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(24, 24, 24, 24)
        main_layout.setSpacing(20)

        # --- 模块 1: 标题栏 ---
        self.title_label = QLabel("均价计算器")
        self.title_label.setObjectName("TitleLabel")
        main_layout.addWidget(self.title_label)

        # --- 模块 2: 输入卡片 ---
        input_card = QFrame()
        input_card.setObjectName("InputCard")
        input_card.setProperty("class", "Card")
        self.add_shadow(input_card)
        
        input_layout = QVBoxLayout(input_card)
        input_layout.setContentsMargins(16, 16, 16, 16)
        input_layout.setSpacing(16)

        # 校验器
        self.validator = QDoubleValidator(0.0, 999999.0, 4)
        self.validator.setNotation(QDoubleValidator.Notation.StandardNotation)

        # 原持仓组
        self.old_price, self.old_weight = self.create_form_row(input_layout, "原持仓", "价格 (元/克)", "重量 (克)")
        
        # 新买入组
        self.new_price, self.new_weight = self.create_form_row(input_layout, "新买入", "价格 (元/克)", "重量 (克)")

        main_layout.addWidget(input_card)

        # --- 模块 3: 操作按钮 ---
        btn_layout = QHBoxLayout()
        btn_layout.setSpacing(12)

        self.clear_btn = QPushButton("清空")
        self.clear_btn.setObjectName("ClearButton")
        self.clear_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.clear_btn.clicked.connect(self.clear_inputs)
        btn_layout.addWidget(self.clear_btn, 1)

        self.calc_btn = QPushButton("开始计算")
        self.calc_btn.setObjectName("CalcButton")
        self.calc_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.calc_btn.clicked.connect(self.calculate)
        btn_layout.addWidget(self.calc_btn, 2)

        main_layout.addLayout(btn_layout)

        # --- 模块 4: 结果卡片 ---
        self.result_card = QFrame()
        self.result_card.setObjectName("ResultCard")
        self.add_shadow(self.result_card, blur=12, opacity=30)
        
        result_layout = QVBoxLayout(self.result_card)
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

        main_layout.addWidget(self.result_card)

        # --- 模块 5: 历史记录 ---
        self.history_title = ClickableLabel("最近记录 ▼")
        self.history_title.setObjectName("HistoryToggleLabel")
        self.history_title.clicked.connect(self.toggle_history)
        main_layout.addWidget(self.history_title)

        self.history_list = QListWidget()
        self.history_list.setSelectionMode(QAbstractItemView.SelectionMode.NoSelection)
        self.history_list.setFixedHeight(120)
        main_layout.addWidget(self.history_list)

        main_layout.addStretch()

    def toggle_history(self):
        """切换历史记录的显示/隐藏"""
        self.history_visible = not self.history_visible
        self.history_list.setVisible(self.history_visible)
        
        if self.history_visible:
            self.history_title.setText("最近记录 ▲")
            self.setFixedHeight(self.height_shown)
        else:
            self.history_title.setText("最近记录 ▼")
            self.setFixedHeight(self.height_hidden)

    def create_form_row(self, parent_layout, group_title, label1, label2):
        group_label = QLabel(group_title)
        group_label.setProperty("class", "Label")
        group_label.setStyleSheet("font-weight: 600; color: #374151; margin-bottom: 2px;")
        parent_layout.addWidget(group_label)

        row_layout = QHBoxLayout()
        row_layout.setSpacing(12)

        # 项 1
        vbox1 = QVBoxLayout()
        vbox1.setSpacing(6)
        l1 = QLabel(label1)
        l1.setProperty("class", "Label")
        e1 = QLineEdit()
        e1.setPlaceholderText("0.00")
        e1.setValidator(self.validator)
        vbox1.addWidget(l1)
        vbox1.addWidget(e1)
        row_layout.addLayout(vbox1)

        # 项 2
        vbox2 = QVBoxLayout()
        vbox2.setSpacing(6)
        l2 = QLabel(label2)
        l2.setProperty("class", "Label")
        e2 = QLineEdit()
        e2.setPlaceholderText("0.00")
        e2.setValidator(self.validator)
        vbox2.addWidget(l2)
        vbox2.addWidget(e2)
        row_layout.addLayout(vbox2)

        parent_layout.addLayout(row_layout)
        return e1, e2

    def add_shadow(self, widget, blur=12, opacity=13):
        shadow = QGraphicsDropShadowEffect()
        shadow.setBlurRadius(blur)
        shadow.setColor(QColor(0, 0, 0, opacity))
        shadow.setOffset(0, 4)
        widget.setGraphicsEffect(shadow)

    def clear_inputs(self):
        self.old_price.clear()
        self.old_weight.clear()
        self.new_price.clear()
        self.new_weight.clear()
        self.result_price_label.setText("均价: --")
        self.result_total_label.setText("总持有量: -- 克")

    def calculate(self):
        try:
            # 输入校验 (如果 validator 没拦住)
            op_text = self.old_price.text().strip()
            ow_text = self.old_weight.text().strip()
            np_text = self.new_price.text().strip()
            nw_text = self.new_weight.text().strip()

            if not all([op_text, ow_text, np_text, nw_text]):
                self.result_price_label.setText("请输入数据")
                self.result_total_label.setText("四个输入框均不能为空")
                return

            op = float(op_text)
            ow = float(ow_text)
            np = float(np_text)
            nw = float(nw_text)

            total_cost = (op * ow) + (np * nw)
            total_weight = ow + nw

            if total_weight > 0:
                avg_price = total_cost / total_weight
                # 格式化保留2位
                avg_price_str = f"￥{avg_price:.2f}"
                total_weight_str = f"总持有量: {total_weight:.2f} 克"
                
                self.result_price_label.setText(avg_price_str)
                self.result_total_label.setText(total_weight_str)

                # 保存历史记录
                self.save_to_history(f"{op:.2f}/{ow:.2f} + {np:.2f}/{nw:.2f} → {avg_price:.2f}")
            else:
                self.result_price_label.setText("均价: --")
                self.result_total_label.setText("总持有量: -- 克")
        except ValueError:
            self.result_price_label.setText("输入非法")
            self.result_total_label.setText("请输入有效的数字格式")

    def save_to_history(self, record_text):
        history = self.settings.value("history", [])
        if not isinstance(history, list): history = []
        
        # 插入到最前面，保持最近的在上面
        history.insert(0, record_text)
        # 只保留最近 5 条
        history = history[:5]
        
        self.settings.setValue("history", history)
        self.update_history_ui(history)

    def load_history(self):
        history = self.settings.value("history", [])
        if not isinstance(history, list): history = []
        self.update_history_ui(history)

    def update_history_ui(self, history):
        self.history_list.clear()
        for item in history:
            self.history_list.addItem(item)

if __name__ == "__main__":
    from PyQt6.QtWidgets import QApplication
    app = QApplication(sys.argv)
    window = GoldCalculatorWindow()
    window.show()
    sys.exit(app.exec())
