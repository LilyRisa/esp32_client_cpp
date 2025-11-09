#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>

enum AudioSourceCM
{
    NONE,
    BLUETOOTH,
    WIFI_STREAM
};

void initAudioManager();
void switchSource(AudioSourceCM src);
AudioSourceCM getCurrentSource();
String getCurrentSourceName();

#endif