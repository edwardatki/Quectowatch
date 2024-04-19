import displayio
from adafruit_display_text.label import Label
from terminalio import FONT
from adafruit_bitmap_font import bitmap_font
from adafruit_display_shapes.line import Line

str_week_days = ("Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun")
str_months = ("January", "Febuary", "March", "April", "May", "June", "July", "August", "September", "November", "December")

str_hours = ("Twelve", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine", "Ten", "Eleven")

str_ones = ("", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine")
str_teens = ("", "Eleven", "Twelve", "Thirteen", "Fourteen", "Fifteen", "Sixteen", "Seventeen", "Eighteen", "Nineteen")
str_tens = ("", "Ten", "Twenty", "Thirty", "Fourty", "Fifty", "Sixty", "Seventy", "Eighty", "Ninety")
str_oclock = "O'Clock"

class Watchface(displayio.Group):
    def __init__(self):
        super().__init__()

        # Text
        self.time_label = Label(font=FONT, text="hour\nminute", anchor_point=(0.0, 0.0), anchored_position=(4, 0), scale=2, line_spacing=1.0, color=0x000000)
        self.date_label = Label(font=FONT, text="month day", anchor_point=(0.0, 1.0), anchored_position=(4, 126), scale=2, line_spacing=1.0, color=0x000000)

        self.div_line = Line(0, 76, 128, 76, color=0x000000)

        # Background
        color_bitmap = displayio.Bitmap(128, 128, 1)
        color_palette = displayio.Palette(1)
        color_palette[0] = 0xFFFFFF
        bg_sprite = displayio.TileGrid(color_bitmap, pixel_shader=color_palette, x=0, y=0)

        # Setup watchface group
        self.append(bg_sprite)
        self.append(self.time_label)
        self.append(self.date_label)
        self.append(self.div_line)

    def update_time(self, t):
        self.time_label.text = "%s" % (str_hours[t.tm_hour % 12])
        
        minute_tens = int(t.tm_min / 10)
        minute_ones = t.tm_min % 10
        
        if minute_tens == 0 and minute_ones == 0:
            self.time_label.text += "\n%s" % (str_oclock)
        elif minute_tens == 0:
            self.time_label.text += "\n%s" % (str_ones[minute_ones])
        elif minute_tens == 1 and minute_ones != 0:
            self.time_label.text += "\n%s" % (str_teens[minute_ones])
        else:
            self.time_label.text += "\n%s" % (str_tens[minute_tens])
            self.time_label.text += "\n%s" % (str_ones[minute_ones])
        
        self.date_label.text = "%s %02d\n%s" % (str_week_days[t.tm_wday], t.tm_mday, str_months[t.tm_mon])

    def update_battery(self, percentage):
        pass
