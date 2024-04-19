import time
import board
import busio
from adafruit_max1704x import MAX17048
from adafruit_ds3231 import DS3231
import displayio
import framebufferio
import sharpdisplay
from adafruit_display_text.label import Label

import time
import board
from adafruit_ble import BLERadio
from adafruit_ble.advertising.standard import ProvideServicesAdvertisement
from adafruit_ble.services.nordic_modded import UARTService
import json
import re

# Start BLE
ble = BLERadio()
uart = UARTService()
advertisement = ProvideServicesAdvertisement(uart)

# BLE variables
lines = list()
notifications = dict()

# Define peripherals
fg = MAX17048(busio.I2C(board.FG_SCL, board.FG_SDA, frequency=400000))
rtc = DS3231(busio.I2C(board.RTC_SCL, board.RTC_SDA, frequency=400000))

# Release the existing display, if any
displayio.release_displays()
      
# Define LCD
lcd_bus = busio.SPI(board.LCD_SCLK, board.LCD_SDI)
framebuffer = sharpdisplay.SharpMemoryFramebuffer(lcd_bus, board.LCD_CS, 128, 128, baudrate=2000000)
display = framebufferio.FramebufferDisplay(framebuffer, rotation=180, auto_refresh=False)

from watchfaces.seven_seg import Watchface

watchface = Watchface()

display.root_group = watchface

was_ble_connected = False
print("Advertising...")
ble.start_advertising(advertisement)
i = 0
while True:
    if was_ble_connected and not ble.connected:
        print("Disconnected!")
        notifications.clear()
        print("Advertising...")
        ble.start_advertising(advertisement)
    
    if not was_ble_connected and ble.connected:
        print("Connected!")
        ble.stop_advertising()
        
    was_ble_connected = ble.connected
    
    if ble.connected:
        if uart.in_waiting > 0:
            lines.append(uart.readline())
        else:
            if len(lines) > 0:
                line = lines.pop(len(lines)-1)
                if "GB" in line:
                    try:
                        line = re.search("\((.*?)\)", line).group(1)
                        print("GB JSON command: ", line)
                        obj = json.loads(line)
                        if obj["t"] == "notify":
                            notifications.update({obj["id"]: obj})
                            print(notifications.keys())
                        elif obj["t"] == "notify-":
                            notifications.pop(obj["id"])
                            print(notifications.keys())
                        else:
                            print("Unknown command")
                    except:
                        print("GB EXCEPTION!")
                else:
                    print("Non-GB command: ", line)

    t = rtc.datetime
    watchface.update_time(t)


    if (i > 10):
        watchface.battery_label.text = ("  Ed Atkinson      %3.0f%%" % (fg.cell_percent))
        i = 0
    i += 1
    
    if (len(notifications) > 0):
        n = notifications.popitem()
        watchface.notification_label.text = n[1]["src"] + "\t\t\t\t\n" + n[1]["title"]
        notifications.update({n[0]: n[1]})
    else:
        watchface.notification_label.text = ""
    
    display.refresh()