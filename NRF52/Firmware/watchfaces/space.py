import displayio
from adafruit_display_text.label import Label
from adafruit_display_shapes.rect import Rect
from terminalio import FONT
from adafruit_bitmap_font import bitmap_font
from adafruit_display_shapes.circle import Circle
from random import randrange, uniform

#class Star():
#    def __init__(self):
#        self.x = randrange(-64, 64)
#        self.y = 64#randrange(32, 64)
#        if randrange(2) > 0:
#            self.y = -self.y
#        self.z = uniform(1.0, 3.0)
#        self.circle = Circle(self.x, self.y, 1, stroke=0, fill=0xFFFFFF)
#    
#    def update(self):
#        sx = (self.x / self.z)
#        sy = (self.y / self.z)
#        
#        self.circle.x = int(sx)+64
#        self.circle.y = int(sy)+64
#        
#        self.z = self.z - 0.1
#        if self.z < 1.0:
#            #self.x = randrange(-64, 64)
#            #self.y = randrange(-64, 64)
#            self.z = 3.0
        

class Watchface(displayio.Group):
    def __init__(self):
        super().__init__()  
        
        # Spawn stars
        # 40 pixel band at top and bottom
        # Also a 5 pixel margin around edges
        #self.stars = []
        #for i in range(20):
        #    star = Star()
        #    self.append(star.circle)
        #    self.stars.append(star)
            
        font_big = bitmap_font.load_font("fonts/Cooper Black-36-r.bdf")
        font_small = bitmap_font.load_font("fonts/Cooper Black-18-r.bdf")

        # Text
        self.time_label = Label(font=font_big, text="00:00", anchor_point=(0.5, 1.0), anchored_position=(64, 72), scale=1, line_spacing=1.0, color=0xffffff)
        self.date_label = Label(font=font_small, text="15/07/24", anchor_point=(0.5, 0.0), anchored_position=(64, 76), scale=1, line_spacing=1.0, color=0xffffff)
        self.append(self.time_label)
        self.append(self.date_label)
        
    def update_time(self, t):
        self.time_label.text = "%02d:%02d" % (t.tm_hour, t.tm_min)
        #for s in self.stars:
        #    s.update()
        return

    def update_battery(self, percentage):
        return
