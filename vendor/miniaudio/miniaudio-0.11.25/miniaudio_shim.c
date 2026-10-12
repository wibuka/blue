#include "miniaudio.h"
#include <stdlib.h>

ma_waveform* ma_waveform_alloc(void)
{
    return malloc(sizeof(ma_waveform));
}

ma_decoder* ma_decoder_alloc(void)
{
    return malloc(sizeof(ma_decoder));
}

ma_engine* ma_engine_alloc(void)
{
    return malloc(sizeof(ma_engine));
}

ma_sound* ma_sound_alloc(void)
{
    return malloc(sizeof(ma_sound));
}
