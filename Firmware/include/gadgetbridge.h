#ifndef _GADGETBRIDGE_H_
#define _GADGETBRIDGE_H_

extern int bluetooth_connected;
extern int music_playing;

void gadgetbridge_send(const char *message);
void gadgetbridge_init();

#endif