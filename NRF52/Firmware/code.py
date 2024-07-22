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
from fruity_menu.menu import Menu
import gc
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
framebuffer = sharpdisplay.SharpMemoryFramebuffer(lcd_bus, board.LCD_CS, 128, 128, baudrate=12000000)
display = framebufferio.FramebufferDisplay(framebuffer, rotation=180, auto_refresh=False)

gadgetbridge = Gadgetbridge()

active_watchface = watchfaces.seven_seg.Watchface()
exit_menu_flag = False
in_menu = False

menu = Menu(display, 128, 128, title="Main Menu")

def exit_menu():
    global exit_menu_flag
    exit_menu_flag = True

def set_watchface(new_watchface):
    global active_watchface
    del active_watchface
    gc.collect()
    active_watchface = new_watchface()
    exit_menu()

watchface_select_menu = menu.create_menu("Watchfaces")
watchface_select_menu.add_action_button("Seven segment", action=set_watchface, args=watchfaces.seven_seg.Watchface)
watchface_select_menu.add_action_button("Words", action=set_watchface, args=watchfaces.words.Watchface)
watchface_select_menu.add_action_button("Analog", action=set_watchface, args=watchfaces.analog.Watchface)
menu.add_submenu_button("Choose watchface", watchface_select_menu)

menu.add_action_button("Exit", action=exit_menu)

i = 0
while True:
    # Notifications
    gadgetbridge.update()
    if (len(gadgetbridge.notifications) > 0):
        n = gadgetbridge.latest_notification()
        active_watchface.notification_label.text = n["src"] + "\t\t\t\t\n" + n["title"]
    else:
        active_watchface.notification_label.text = ""
    
    buttons.update()
    
    if exit_menu_flag:
        in_menu = False
        menu.reset_menu()
        exit_menu_flag = False
    
    if in_menu:
        if buttons.up_just_pressed:
            menu.scroll(-1)
            menu.show_menu()
            display.refresh()
        if buttons.down_just_pressed:
            menu.scroll(1)
            menu.show_menu()
            display.refresh()
        if buttons.center_just_pressed:
            menu.click()
            menu.show_menu()
            display.refresh()
        
        #time.sleep(1.0/30.0)
    else:
        if buttons.center_just_long_pressed:
            in_menu = True
            menu.show_menu()
            display.refresh()
            buttons.update()
            continue
    
        if buttons.up_just_long_pressed:
            gadgetbridge.send_info_message("test")
    
        if buttons.down_just_long_pressed:
            gadgetbridge.clear_latest_notification()
        
        display.root_group = active_watchface

        # Update watchface time
        t = rtc.datetime
        active_watchface.update_time(t)

        # Update watchface battery reading, fuel gauge is kinda slow so only do occasionally
        if (i > 100):
            active_watchface.update_battery(fg.cell_percent)
            i = 0
        i += 1
        
        # Redraw display
        display.refresh()
    
        #time.sleep(1.0/10.0)