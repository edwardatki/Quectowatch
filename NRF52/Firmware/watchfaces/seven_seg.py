import displayio
from adafruit_display_text.label import Label
from terminalio import FONT
from adafruit_bitmap_font import bitmap_font

week_days = ("Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun")
months = ("Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Nov", "Dec")

class Watchface(displayio.Group):
    def __init__(self):
        super().__init__()
        
        seven_seg_font_big = bitmap_font.load_font("fonts/DSEG7ClassicMini-Bold-26.bdf")
        seven_seg_font_small = bitmap_font.load_font("fonts/DSEG7ClassicMini-Bold-13.bdf")

        # Text
        self.time_label = Label(font=seven_seg_font_big, text="00:00", anchor_point=(0.5, 0.5), anchored_position=(64, 60), scale=1, line_spacing=1.0, color=0x000000, background_color=0xffffff)
        self.second_label = Label(font=seven_seg_font_small, text="00", anchor_point=(1.0, 0.0), anchored_position=(124, 18), scale=1, line_spacing=1.0, color=0x000000, background_color=0xffffff)
        self.dow_label = Label(font=FONT, text="???", anchor_point=(0.0, 0.0), anchored_position=(8, 14), scale=2, line_spacing=1.0, color=0x000000, background_color=0xffffff)
        self.date_label = Label(font=seven_seg_font_small, text="???", anchor_point=(0.5, 0.0), anchored_position=(64, 64+20), scale=1, line_spacing=1.0, color=0x000000, background_color=0xffffff)
        self.battery_label = Label(font=FONT, text="", anchor_point=(1.0, 0.0), anchored_position=(128, 0), scale=1, line_spacing=1.0, color=0xffffff, background_color=0x000000)
        self.notification_label = Label(font=FONT, text="", anchor_point=(0.0, 1.0), anchored_position=(0, 128), scale=1, line_spacing=1.0, color=0xffffff, background_color=0x000000)

        # Background
        color_bitmap = displayio.Bitmap(128, 128, 1)
        color_palette = displayio.Palette(1)
        color_palette[0] = 0xFFFFFF
        bg_sprite = displayio.TileGrid(color_bitmap, pixel_shader=color_palette, x=0, y=0)

        # Setup watchface group
        self.append(bg_sprite)
        self.append(self.time_label)
        self.append(self.second_label)
        self.append(self.dow_label)
        self.append(self.date_label)
        self.append(self.battery_label)
        self.append(self.notification_label)

    def update_time(self, t):
        self.time_label.text = "%02d%c%02d" % (t.tm_hour, ":" if (t.tm_sec % 2) else " ", t.tm_min)
        self.second_label.text = "%02d" % (t.tm_sec)
        self.date_label.text = "%02d-%02d-%02d" % (t.tm_mday, t.tm_mon, t.tm_year % 100)
        self.dow_label.text = "%s" % (week_days[t.tm_wday])

    def update_battery(self, percentage):
        self.battery_label.text = ("  Ed Atkinson      %3.0f%%" % (percentage))