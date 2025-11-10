#include <stdint.h>
#include <time.h>
#include <sound.h>


uint8_t spkIn(uint16_t port);
void spkOut(uint16_t port, uint8_t value);

void stopSpeaker()
{
    uint8_t tmp = spkIn(0x61) & 0xFC;
    spkOut(0x61, tmp);
}

void triggerSpeaker(uint32_t frequence)
{
    uint32_t Div;
    uint8_t tmp;

    if (frequence == 0)
    {
        stopSpeaker();
        return;
    }

    // Set the PIT to the desired frequency
    Div = 1193180 / frequence;
    spkOut(0x43, 0xb6);
    spkOut(0x42, (uint8_t)(Div));
    spkOut(0x42, (uint8_t)(Div >> 8));

    // And play the sound using the PC speaker
    tmp = spkIn(0x61);
    if (tmp != (tmp | 3))
    {
        spkOut(0x61, tmp | 3);
    }
}

void beep(uint32_t freq, uint64_t duration)
{
    triggerSpeaker(freq);
    sleep(duration);
    stopSpeaker();
}