import time
import board
import busio
from adafruit_ds3231 import DS3231

i2c = busio.I2C(board.RTC_SCL, board.RTC_SDA)
rtc = DS3231(i2c)

rtc.datetime = time.struct_time((2024,4,20,00,24,00,5,9,-1))