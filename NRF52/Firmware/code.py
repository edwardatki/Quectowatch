import time
import board
import busio
from adafruit_max1704x import MAX17048
from adafruit_ds3231 import DS3231
import displayio
import framebufferio
import sharpdisplay

from watchfaces.seven_seg import Watchface
from gadgetbridge import Gadgetbridge

# Define peripherals
fg = MAX17048(busio.I2C(board.FG_SCL, board.FG_SDA, frequency=400000))
rtc = DS3231(busio.I2C(board.RTC_SCL, board.RTC_SDA, frequency=400000))

# Release the existing display, if any
displayio.release_displays()
      
# Define LCD
lcd_bus = busio.SPI(board.LCD_SCLK, board.LCD_SDI)
framebuffer = sharpdisplay.SharpMemoryFramebuffer(lcd_bus, board.LCD_CS, 128, 128, baudrate=2000000)
display = framebufferio.FramebufferDisplay(framebuffer, rotation=180, auto_refresh=False)

gadgetbridge = Gadgetbridge()

watchface = Watchface()

display.root_group = watchface

i = 0
while True:
    gadgetbridge.update()

    t = rtc.datetime
    watchface.update_time(t)

    if (i > 10):
        watchface.battery_label.text = ("  Ed Atkinson      %3.0f%%" % (fg.cell_percent))
        i = 0
    i += 1
    
    if (len(gadgetbridge.notifications) > 0):
        watchface.notification_label.text = gadgetbridge.latest_notification()["src"] + "\t\t\t\t\n" + n[1]["title"]
    else:
        watchface.notification_label.text = ""
    
    display.refresh()