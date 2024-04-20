import time
import board
import busio
from adafruit_max1704x import MAX17048
from adafruit_ds3231 import DS3231
import displayio
import framebufferio
import sharpdisplay
import buttons
from gadgetbridge import Gadgetbridge
import watchfaces.seven_seg
import watchfaces.analog
import watchfaces.words

# Define peripherals
fg = MAX17048(busio.I2C(board.FG_SCL, board.FG_SDA, frequency=400000))
rtc = DS3231(busio.I2C(board.RTC_SCL, board.RTC_SDA, frequency=400000))

# Release the existing display, if any
displayio.release_displays()
      
# Define LCD
lcd_bus = busio.SPI(board.LCD_SCLK, board.LCD_SDI)
framebuffer = sharpdisplay.SharpMemoryFramebuffer(lcd_bus, board.LCD_CS, 128, 128, baudrate=8000000)
display = framebufferio.FramebufferDisplay(framebuffer, rotation=180, auto_refresh=False)

watchfaces = (watchfaces.seven_seg.Watchface(), watchfaces.words.Watchface(), watchfaces.analog.Watchface())
watchface_index = 0
display.root_group = watchfaces[watchface_index]

#gadgetbridge = Gadgetbridge()

i = 0
while True:
    # Notifications
    #gadgetbridge.update()
    #if (len(gadgetbridge.notifications) > 0):
    #    watchface.notification_label.text = gadgetbridge.latest_notification()["src"] + "\t\t\t\t\n" + n[1]["title"]
    #else:
    #    watchface.notification_label.text = ""

    buttons.update()
    if buttons.up_just_pressed:
        watchface_index = (watchface_index + 1) % len(watchfaces)
    if buttons.down_just_pressed:
        watchface_index = (watchface_index - 1) % len(watchfaces)
    
    watchface = watchfaces[watchface_index]
    display.root_group = watchfaces[watchface_index]

    # Update watchface time
    t = rtc.datetime
    watchfaces[watchface_index].update_time(t)

    # Update watchface battery reading, fuel gauge is kinda slow so only do occasionally
    if (i > 10):
        watchfaces[watchface_index].update_battery(fg.cell_percent)
        i = 0
    i += 1
    
    # Redraw display
    display.refresh()