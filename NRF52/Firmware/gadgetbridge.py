from adafruit_ble import BLERadio
from adafruit_ble.advertising.standard import ProvideServicesAdvertisement
from adafruit_ble.services.nordic_modded import UARTService
import json
import re

class Gadgetbridge:
    def __init__(self):
        # Start BLE
        self.ble = BLERadio()
        self.uart = UARTService()
        self.advertisement = ProvideServicesAdvertisement(self.uart)

        # BLE variables
        self.lines = list()
        self.notifications = dict()

        self.was_ble_connected = False
        print("Advertising...")
        self.ble.start_advertising(self.advertisement)
        
    def update(self):
        if self.was_ble_connected and not self.ble.connected:
            print("Disconnected!")
            #self.notifications.clear()
            print("Advertising...")
            self.ble.start_advertising(self.advertisement)
        
        if not self.was_ble_connected and self.ble.connected:
            print("Connected!")
            self.ble.stop_advertising()
            
        self.was_ble_connected = self.ble.connected
        
        if self.ble.connected:
            if self.uart.in_waiting > 0:
                self.lines.append(self.uart.readline())
            else:
                if len(self.lines) > 0:
                    line = self.lines.pop(len(self.lines)-1)
                    print("RX: ", line)
                    if "GB" in line:
                        try:
                            line = re.search("\((.*?)\)", line).group(1)
                            print("GB JSON command: ", line)
                            obj = json.loads(line)
                            if obj["t"] == "notify":
                                self.notifications.update({obj["id"]: obj})
                                print(self.notifications.keys())
                            elif obj["t"] == "notify-":
                                self.notifications.pop(obj["id"])
                                print(self.notifications.keys())
                            else:
                                print("Unknown command")
                        except:
                            print("GB EXCEPTION!")
                    else:
                        print("Non-GB command: ", line)
    
    def send_info_message(self, msg):
        cmd = "{t:\"info\", msg:\"%s\"} \n" % msg
        self.uart.write(cmd.encode("utf-8"))
        print(cmd.encode("utf-8"))
    
    def latest_notification(self):
        if len(self.notifications) == 0:
            return None
        
        n = self.notifications.popitem()
        self.notifications.update({n[0]: n[1]})
        return n[1]
    
    def clear_latest_notification(self):
        if len(self.notifications) == 0:
            return

        n = self.notifications.popitem()
        self.notifications.update({n[0]: n[1]})
        self.clear_notification(n[0])
    
    def clear_notification(self, id):
        self.notifications.pop(id)
        cmd = "{t:\"notify\", id:%d, n:\"DISMISS\"} \n" %  id
        self.uart.write(cmd.encode("utf-8"))
        print(cmd.encode("utf-8"))
        
