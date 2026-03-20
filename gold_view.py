import sys
import requests
import time
import re
from PyQt6.QtWidgets import (QApplication, QMainWindow, QLabel, QVBoxLayout, 
                             QWidget, QFrame, QGraphicsDropShadowEffect, QMenu, QSystemTrayIcon)
from PyQt6.QtCore import Qt, QTimer, QThread, pyqtSignal, QPoint, QPropertyAnimation, QEasingCurve, QRect, QSize
from PyQt6.QtGui import QColor, QFont, QAction, QIcon, QCursor, QGuiApplication, QPixmap, QPainter
from PyQt6.QtSvg import QSvgRenderer
from calculator import GoldCalculatorWindow

# --- 实时金价获取线程 ---
class GoldPriceThread(QThread):
    price_updated = pyqtSignal(float, float, float)  # 当前价, 开盘价, 最低价

    def run(self):
        while True:
            price, open_price, low_price = self.fetch_data()
            if price > 0:
                self.price_updated.emit(price, open_price, low_price)
            # 用户要求 0.5 秒刷新一次 (注意：频繁请求可能被 API 封禁，这里保持用户要求)
            time.sleep(0.5)

    def fetch_data(self):
        # 接口 1: 新浪财经 (主接口)
        try:
            url = "https://hq.sinajs.cn/list=hf_XAU"
            headers = {"Referer": "http://finance.sina.com.cn"}
            response = requests.get(url, headers=headers, timeout=2)
            content = response.content.decode('gbk')
            match = re.search(r'"(.*)"', content)
            if match:
                data = match.group(1).split(',')
                if len(data) > 10:
                    price = float(data[0])      # 当前价
                    low = float(data[5])        # 最低价
                    open_p = float(data[8])     # 开盘价
                    return price, open_p, low
        except:
            pass

        # 接口 2: Gold-API (备用接口)
        try:
            response = requests.get("https://api.gold-api.com/v1/gold", timeout=2)
            if response.status_code == 200:
                data = response.json()
                price = data.get("price", 0.0)
                return price, price, price 
        except:
            pass

        return 0.0, 0.0, 0.0

# --- 极简矩形悬浮窗口 ---
class GoldFloatingWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        
        # 窗口基本设置
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint |       # 无边框
            Qt.WindowType.WindowStaysOnTopHint |     # 窗口置顶
            Qt.WindowType.Tool                        # 不在任务栏显示
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground) # 透明背景
        
        # 极简矩形尺寸：略微调大以提高可读性
        self.window_width = 95
        self.window_height = 38
        self.setFixedSize(self.window_width, self.window_height)
        
        self.m_drag = False
        self.m_DragPosition = QPoint()
        self.is_hidden = False
        self.edge_side = None
        self.normal_geometry = None
        self.is_animating = False
        self.calc_window = None # 计算器窗口实例
        
        # 定时器：延迟隐藏处理
        self.hide_timer = QTimer()
        self.hide_timer.setSingleShot(True)
        self.hide_timer.timeout.connect(self.hide_to_edge)
        self.hide_delay = 2000 # 靠近边缘 2 秒后自动隐藏
        
        self.init_ui()
        self.init_tray()
        
        # 定时器：检测鼠标位置实现自动弹出
        self.check_timer = QTimer()
        self.check_timer.timeout.connect(self.check_mouse_pos)
        self.check_timer.start(200)
        
        self.gold_thread = GoldPriceThread()
        self.gold_thread.price_updated.connect(self.update_display)
        self.gold_thread.start()

    def create_tray_icon(self):
        # 创建一个淡绿色金块的 SVG 图标
        svg_content = """
        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
          <path d="M10,80 L25,30 L75,30 L90,80 Z" fill="#90EE90" stroke="#2E8B57" stroke-width="3"/>
          <path d="M25,30 L35,20 L65,20 L75,30 Z" fill="#C1FFC1" stroke="#2E8B57" stroke-width="2"/>
          <line x1="30" y1="45" x2="70" y2="45" stroke="#2E8B57" stroke-width="2" stroke-linecap="round"/>
          <line x1="25" y1="60" x2="75" y2="60" stroke="#2E8B57" stroke-width="2" stroke-linecap="round"/>
        </svg>
        """
        renderer = QSvgRenderer(svg_content.encode('utf-8'))
        pixmap = QPixmap(QSize(64, 64))
        pixmap.fill(Qt.GlobalColor.transparent)
        painter = QPainter(pixmap)
        renderer.render(painter)
        painter.end()
        return QIcon(pixmap)

    def init_ui(self):
        # 主容器
        self.container = QFrame(self)
        self.container.setObjectName("MainContainer")
        # 留出 2px 给阴影或边框空间
        self.container.setFixedSize(self.window_width - 4, self.window_height - 4)
        self.container.move(2, 2)
        
        # 初始应用正常样式
        self.apply_normal_style()
        
        layout = QVBoxLayout(self.container)
        layout.setContentsMargins(4, 0, 4, 0)
        layout.setSpacing(0)
        
        self.price_label = QLabel("-.--")
        self.price_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        
        layout.addWidget(self.price_label)
        self.setCentralWidget(self.container)

    def apply_normal_style(self):
        """正常显示状态下的极简风格"""
        self.container.setFixedSize(self.window_width - 4, self.window_height - 4)
        self.container.move(2, 2)
        self.container.setStyleSheet("""
            #MainContainer {
                background-color: rgba(255, 255, 255, 45);
                border-radius: 0px;
                border: 0.5px solid rgba(255, 255, 255, 120);
            }
            QLabel {
                background: transparent;
                color: #000000;
                font-family: 'Microsoft YaHei UI', 'Segoe UI', 'SimSun', sans-serif;
                font-weight: normal;
                font-size: 18px;
            }
        """)

    def apply_hidden_style(self):
        """隐藏状态下的白色线条风格"""
        # 隐藏时让容器填满窗口，确保白色线条紧贴屏幕边缘
        self.container.setFixedSize(self.window_width, self.window_height)
        self.container.move(0, 0)
        self.container.setStyleSheet("""
            #MainContainer {
                background-color: rgba(255, 255, 255, 200);
                border-radius: 0px;
                border: none;
            }
            QLabel {
                color: transparent;
            }
        """)

    def init_tray(self):
        self.tray_icon = QSystemTrayIcon(self)
        self.tray_icon.setIcon(self.create_tray_icon())
        
        menu = QMenu()
        
        # 均价计算功能
        calc_action = QAction("均价计算", self)
        calc_action.triggered.connect(self.open_calculator)
        menu.addAction(calc_action)
        
        menu.addSeparator()
        
        quit_action = QAction("退出程序", self)
        quit_action.triggered.connect(QApplication.instance().quit)
        menu.addAction(quit_action)
        
        self.tray_icon.setContextMenu(menu)
        self.tray_icon.show()

    def open_calculator(self):
        """打开均价计算器窗口"""
        if self.calc_window is None:
            self.calc_window = GoldCalculatorWindow()
        self.calc_window.show()
        self.calc_window.raise_()
        self.calc_window.activateWindow()

    def contextMenuEvent(self, event):
        """右键菜单"""
        menu = QMenu(self)
        
        calc_action = QAction("均价计算", self)
        calc_action.triggered.connect(self.open_calculator)
        menu.addAction(calc_action)
        
        menu.addSeparator()
        
        quit_action = QAction("退出程序", self)
        quit_action.triggered.connect(QApplication.instance().quit)
        menu.addAction(quit_action)
        
        menu.exec(event.globalPos())

    def update_display(self, price, open_p, low_p):
        self.price_label.setText(f"{price:.2f}")

    # --- 交互与边缘逻辑 ---
    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            self.m_drag = True
            self.m_DragPosition = event.globalPosition().toPoint() - self.frameGeometry().topLeft()
            event.accept()

    def mouseMoveEvent(self, event):
        if Qt.MouseButton.LeftButton and self.m_drag:
            target_pos = event.globalPosition().toPoint() - self.m_DragPosition
            
            # 限制逻辑：悬浮窗不能移出屏幕范围
            screen = QGuiApplication.screenAt(event.globalPosition().toPoint())
            if not screen:
                screen = QApplication.primaryScreen()
            screen_geo = screen.geometry()
            
            # 限制 X 轴
            x = max(screen_geo.left(), min(target_pos.x(), screen_geo.right() - self.width()))
            # 限制 Y 轴
            y = max(screen_geo.top(), min(target_pos.y(), screen_geo.bottom() - self.height()))
            
            self.move(x, y)
            event.accept()

    def mouseReleaseEvent(self, event):
        self.m_drag = False
        self.check_edge_status()

    def check_edge_status(self):
        screen = QGuiApplication.screenAt(self.pos())
        if not screen:
            screen = QApplication.primaryScreen()
        screen_geo = screen.geometry()
        
        pos = self.pos()
        x, y = pos.x(), pos.y()
        w, h = self.width(), self.height()
        
        snap_threshold = 12
        
        self.edge_side = None
        # 检查左边缘
        if abs(x - screen_geo.left()) <= snap_threshold:
            self.edge_side = 'left'
            self.move(screen_geo.left(), y) # 自动对齐
        # 检查右边缘
        elif abs(x + w - screen_geo.right()) <= snap_threshold:
            self.edge_side = 'right'
            self.move(screen_geo.right() - w, y) # 自动对齐
        # 检查顶边缘
        elif abs(y - screen_geo.top()) <= snap_threshold:
            self.edge_side = 'top'
            self.move(x, screen_geo.top()) # 自动对齐
            
        if self.edge_side and not self.is_hidden:
            if not self.hide_timer.isActive():
                self.normal_geometry = self.geometry()
                self.hide_timer.start(self.hide_delay)
        else:
            self.hide_timer.stop()

    def hide_to_edge(self):
        """执行隐藏动画，只留出 6px 的边缘感应线"""
        if not self.edge_side or self.is_hidden or self.m_drag: return
        
        # 重新获取窗口所在的屏幕，确保坐标系正确
        screen = QGuiApplication.screenAt(self.geometry().center())
        if not screen:
            screen = QApplication.primaryScreen()
        screen_geo = screen.geometry()
        
        x, y = self.pos().x(), self.pos().y()
        w, h = self.width(), self.height()
        
        # 隐藏状态：留出 6px 粗线，确保醒目
        offset = 6 
        
        if self.edge_side == 'left':
            target = QRect(screen_geo.left() - w + offset, y, w, h)
        elif self.edge_side == 'right':
            target = QRect(screen_geo.right() - offset, y, w, h)
        elif self.edge_side == 'top':
            # 修复：确保在任何屏幕上，顶部隐藏的位置都相对于该屏幕的顶部
            target = QRect(x, screen_geo.top() - h + offset, w, h)
        else:
            return

        self.animate_window(target)
        self.apply_hidden_style() # 应用隐藏样式
        self.is_hidden = True

    def show_normal(self):
        if self.is_hidden and self.normal_geometry:
            self.animate_window(self.normal_geometry)
            self.apply_normal_style() # 恢复正常样式
            self.is_hidden = False

    def check_mouse_pos(self):
        if self.m_drag or self.is_animating: return
        cursor_pos = QCursor.pos()
        window_rect = self.geometry()
        
        sense_rect = window_rect.adjusted(-5, -5, 5, 5)
        is_cursor_in = sense_rect.contains(cursor_pos)
        
        if is_cursor_in:
            self.hide_timer.stop()
            if self.is_hidden:
                self.show_normal()
        else:
            if not self.is_hidden:
                self.check_edge_status()
    
    def animate_window(self, target_rect):
        self.is_animating = True
        self.anim = QPropertyAnimation(self, b"geometry")
        self.anim.setDuration(250)
        self.anim.setStartValue(self.geometry())
        self.anim.setEndValue(target_rect)
        self.anim.setEasingCurve(QEasingCurve.Type.OutCubic)
        self.anim.finished.connect(self.on_animation_finished)
        self.anim.start()

    def on_animation_finished(self):
        self.is_animating = False

if __name__ == "__main__":
    app = QApplication(sys.argv)
    # 修复：设置当最后一个窗口关闭时不退出程序
    app.setQuitOnLastWindowClosed(False)
    # 确保支持高 DPI
    app.setApplicationName("GoldView")
    
    window = GoldFloatingWindow()
    
    # 识别多块屏幕，找到最右侧的那块
    screens = QGuiApplication.screens()
    target_screen = screens[0]
    max_right = target_screen.geometry().right()
    
    for s in screens:
        if s.geometry().right() > max_right:
            max_right = s.geometry().right()
            target_screen = s
            
    screen_geo = target_screen.geometry()
    # 初始位置：最右侧屏幕的右边缘，向下偏移 100px
    window.move(screen_geo.right() - window.width() - 5, screen_geo.top() + 100)
    
    window.show()
    sys.exit(app.exec())
