import displayio
from adafruit_display_text.label import Label
from terminalio import FONT
from adafruit_display_shapes.circle import Circle
from adafruit_display_shapes.line import Line
import math

class Watchface(displayio.Group):
    second_hand_length = 40
    minute_hand_length = 32
    hour_hand_length = 24

    def __init__(self):
        super().__init__()

        self.face = Circle(64, 64, 63, stroke=0, fill=0xFFFFFF)
        self.center = Circle(64, 64, 2, stroke=0, fill=0x000000)
        self.twelve_label = Label(font=FONT, text="12", anchor_point=(0.5, 0.0), anchored_position=(64, 4), scale=2, line_spacing=1.0, color=0x000000, background_color=0xffffff)
        self.three_label = Label(font=FONT, text="3", anchor_point=(1.0, 0.5), anchored_position=(124, 64), scale=2, line_spacing=1.0, color=0x000000, background_color=0xffffff)
        self.six_label = Label(font=FONT, text="6", anchor_point=(0.5, 1.0), anchored_position=(64, 124), scale=2, line_spacing=1.0, color=0x000000, background_color=0xffffff)
        self.nine_label = Label(font=FONT, text="9", anchor_point=(0.0, 0.5), anchored_position=(4, 64), scale=2, line_spacing=1.0, color=0x000000, background_color=0xffffff)
        
        self.second_hand = Line(64, 64, 0, self.second_hand_length, color=0x000000)
        self.minute_hand = Line(64, 64, 0, self.minute_hand_length, color=0x000000)
        self.hour_hand = Line(64, 64, 0, self.hour_hand_length, color=0x000000)

        # Setup watchface group
        self.append(self.face)
        self.append(self.center)
        self.append(self.twelve_label)
        self.append(self.three_label)
        self.append(self.six_label)
        self.append(self.nine_label)
        self.append(self.second_hand)
        self.append(self.minute_hand)
        self.append(self.hour_hand)

    def update_time(self, t):
        second_angle = (t.tm_sec / 60.0) * 2.0 * math.pi
        self.remove(self.second_hand)
        del self.second_hand
        self.second_hand = Line(64, 64, 64+int(self.second_hand_length * math.sin(second_angle)), 64-int(self.second_hand_length * math.cos(second_angle)), color=0x000000)
        self.append(self.second_hand)
        
        minute_angle = (t.tm_min / 60.0) * 2.0 * math.pi
        self.remove(self.minute_hand)
        del self.minute_hand
        self.minute_hand = Line(64, 64, 64+int(self.minute_hand_length * math.sin(minute_angle)), 64-int(self.minute_hand_length * math.cos(minute_angle)), color=0x000000)
        self.append(self.minute_hand)
        
        hour_angle = (t.tm_hour / 12.0) * 2.0 * math.pi
        self.remove(self.hour_hand)
        del self.hour_hand
        self.hour_hand = Line(64, 64, 64+int(self.hour_hand_length * math.sin(hour_angle)), 64-int(self.hour_hand_length * math.cos(hour_angle)), color=0x000000)
        self.append(self.hour_hand)

    def update_battery(self, percentage):
        pass
