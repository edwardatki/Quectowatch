import board
import digitalio

button_up = digitalio.DigitalInOut(board.BUTTON_UP)
button_up.direction = digitalio.Direction.INPUT
button_up.pull = digitalio.Pull.UP

button_center = digitalio.DigitalInOut(board.BUTTON_CENTER)
button_center.direction = digitalio.Direction.INPUT
button_center.pull = digitalio.Pull.UP

button_down = digitalio.DigitalInOut(board.BUTTON_DOWN)
button_down.direction = digitalio.Direction.INPUT
button_down.pull = digitalio.Pull.UP

up_pressed = False
center_pressed = False
down_pressed = False

up_just_pressed = False
center_just_pressed = False
down_just_pressed = False

was_up_pressed = False
was_center_pressed = False
was_down_pressed = False

def update():
    global up_just_pressed
    global center_just_pressed
    global down_just_pressed
    
    global was_up_pressed
    global was_center_pressed
    global was_down_pressed
    
    up_pressed = button_up.value == 0
    center_pressed = button_center.value == 0
    down_pressed = button_down.value == 0
    
    up_just_pressed = up_pressed and not was_up_pressed
    down_just_pressed = down_pressed and not was_down_pressed
    center_just_pressed = center_pressed and not was_center_pressed

    was_up_pressed = up_pressed
    was_center_pressed = center_pressed
    was_down_pressed = down_pressed
