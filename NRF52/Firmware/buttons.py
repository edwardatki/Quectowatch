import board
import digitalio
import time

long_press_threshold = 1 * 1000000000

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

up_press_start = time.monotonic_ns()
center_press_start = time.monotonic_ns()
down_press_start = time.monotonic_ns()

up_long_pressed = False
center_long_pressed = False
down_long_pressed = False

up_was_long_pressed = False
center_was_long_pressed = False
down_was_long_pressed = False

up_just_long_pressed = False
center_just_long_pressed = False
down_just_long_pressed  = False

def update():
    global up_pressed
    global center_pressed
    global down_pressed

    global up_just_pressed
    global center_just_pressed
    global down_just_pressed

    global was_up_pressed
    global was_center_pressed
    global was_down_pressed

    global up_press_start
    global center_press_start
    global down_press_start

    global up_long_pressed
    global center_long_pressed
    global down_long_pressed

    global up_was_long_pressed
    global center_was_long_pressed
    global down_was_long_pressed

    global up_just_long_pressed
    global center_just_long_pressed 
    global down_just_long_pressed
    
    up_pressed = button_up.value == 0
    center_pressed = button_center.value == 0
    down_pressed = button_down.value == 0
    
    up_just_pressed = up_pressed and not was_up_pressed
    down_just_pressed = down_pressed and not was_down_pressed
    center_just_pressed = center_pressed and not was_center_pressed

    was_up_pressed = up_pressed
    was_center_pressed = center_pressed
    was_down_pressed = down_pressed
    
    now = time.monotonic_ns()
    
    if not up_pressed:
        up_press_start = now
    up_long_pressed = (now - up_press_start) > long_press_threshold
    up_just_long_pressed = up_long_pressed and not up_was_long_pressed
    up_was_long_pressed = up_long_pressed
    
    if not center_pressed:
        center_press_start = now
    center_long_pressed = (now - center_press_start) > long_press_threshold
    center_just_long_pressed = center_long_pressed and not center_was_long_pressed
    center_was_long_pressed = center_long_pressed
    
    if not down_pressed:
        down_press_start = now
    down_long_pressed = (now - down_press_start) > long_press_threshold
    down_just_long_pressed = down_long_pressed and not down_was_long_pressed
    down_was_long_pressed = down_long_pressed
