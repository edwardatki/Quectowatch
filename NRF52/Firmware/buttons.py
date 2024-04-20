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

def up_pressed():
    return !button_up.value