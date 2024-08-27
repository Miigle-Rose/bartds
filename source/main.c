// SPDX-License-Identifier: CC0-1.0
// SPDX-FileContributor: Michele Di Giorgio, 2024

#include <stdio.h>
#include <math.h>

#include <filesystem.h>
#include <maxmod9.h>
#include <nds.h>

#include <stdbool.h>

#include <soundbank.h>

#include <time.h>
#include <stdlib.h>

#include <gl2d.h>

//Sprites
#include "bart.h"
//Bottom Screen
#include "city.h"
#include "bg1.h"
//Top Screen
#include "splash.h"
#include "congrat.h"

touchPosition touch_pos;

#define WAV_FILENAME "nitro:/ssbm_hitthetargets.wav"
#define WAV_FILENAME2 "nitro:/bart.wav"

#define DATA_ID 0x61746164
#define FMT_ID  0x20746d66
#define RIFF_ID 0x46464952
#define WAVE_ID 0x45564157

FILE *wavFile = NULL;

typedef struct WAVHeader {
    // "RIFF" chunk descriptor
    uint32_t chunkID;
    uint32_t chunkSize;
    uint32_t format;
    // "fmt" subchunk
    uint32_t subchunk1ID;
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    // "data" subchunk
    uint32_t subchunk2ID;
    uint32_t subchunk2Size;
} WAVHeader_t;

mm_word streamingCallback(mm_word length,
                          mm_addr dest,
                          mm_stream_formats format)
{
    // 16-bit, 2 channels (stereo), so we need to output 4 bytes for each word
    int res = fread(dest, 2, length, wavFile);

    if (feof(wavFile)) {
        // Loop back when song ends
        fseek(wavFile, sizeof(WAVHeader_t), SEEK_SET);
        res = fread(dest, 2, length, wavFile);
    }
    // Return the number of words streamed so that if this number is less than
    // "length", the rest will be buffered and used in the next stream
    return res;
}

int checkWAVHeader(const WAVHeader_t header)
{
    if (header.chunkID != RIFF_ID) {
        printf("Wrong RIFF_ID %lx\n", header.chunkID);
        return 1;
    }

    if (header.format != WAVE_ID) {
        printf("Wrong WAVE_ID %lx\n", header.format);
        return 1;
    }

    if (header.subchunk1ID != FMT_ID) {
        printf("Wrong FMT_ID %lx\n", header.subchunk1ID);
        return 1;
    }

    if (header.subchunk2ID != DATA_ID) {
        printf("Wrong Subchunk2ID %lx\n", header.subchunk2ID);
        return 1;
    }

    return 0;
}

mm_stream_formats getMMStreamType(uint16_t numChannels, uint16_t bitsPerSample)
{
    if (numChannels == 1) {
        if (bitsPerSample == 8) {
            return MM_STREAM_8BIT_MONO;
        } else {
            return MM_STREAM_16BIT_MONO;
        }
    } else if (numChannels == 2) {
        if (bitsPerSample == 8) {
            return MM_STREAM_8BIT_STEREO;
        } else {
            return MM_STREAM_16BIT_STEREO;
        }
    }
    return MM_STREAM_8BIT_MONO;
}

void waitForever(void)
{
    while (1)
        swiWaitForVBlank();
}

double distance(double x1, double y1, double x2, double y2)
{
    double square_difference_x = (x2 - x1) * (x2 - x1);
    double square_difference_y = (y2 - y1) * (y2 - y1);
    double sum = square_difference_x + square_difference_y;
    double value = sqrt(sum);
    return value;
}

int rand_int(int min, int max){
    int x = rand() % (max - min + 1) + min;
    return x;
}

int main(int argc, char **argv)
{
    srand(time(NULL));

    videoSetMode(MODE_2_2D);
    videoSetModeSub(MODE_2_2D);
    
    vramSetPrimaryBanks(VRAM_A_MAIN_BG, VRAM_B_LCD, VRAM_C_LCD, VRAM_D_SUB_SPRITE);

    int bg = bgInit(2, BgType_Rotation, BgSize_R_256x256, 0, 1);

    // Copy Top Screen
    dmaCopy(splashTiles, bgGetGfxPtr(bg), splashTilesLen);
    dmaCopy(splashMap, bgGetMapPtr(bg), splashMapLen);
    dmaCopy(splashPal, BG_PALETTE, splashPalLen);

    consoleDemoInit();

    oamInit(&oamMain, SpriteMapping_1D_32, false);
    oamInit(&oamSub, SpriteMapping_1D_32, false);

    mmInitDefault("nitro:/soundbank.bin");

    bool initOK = nitroFSInit(NULL);
    if (!initOK) {
        printf("nitroFSInit() error!\n");
        return 1;
    }

    wavFile = fopen(WAV_FILENAME, "rb");
    if (wavFile == NULL) {
        printf("fopen(%s) failed!\n", WAV_FILENAME);
        waitForever();
    }

    WAVHeader_t wavHeader = { 0 };
    if (fread(&wavHeader, 1, sizeof(WAVHeader_t), wavFile) == 0) {
        perror("fread");
        waitForever();
    }
    if (checkWAVHeader(wavHeader) != 0) {
        printf("WAV file header is corrupt!\n");
        waitForever();
    }

    // Open the stream
    mm_stream stream;
    stream.sampling_rate = wavHeader.sampleRate;
    stream.buffer_length = 800;
    stream.callback      = streamingCallback;
    stream.format        = getMMStreamType(wavHeader.numChannels, wavHeader.bitsPerSample);
    stream.timer         = MM_TIMER0;
    stream.manual        = false;
    mmStreamOpen(&stream);

    // Load OW!
    mmLoadEffect(SFX_OW);
    mmEffectVolume(SFX_OW, 255);

    // Load CONGRAT!
    mmLoadEffect(SFX_CONGRAT);
    mmEffectVolume(SFX_CONGRAT, 255);

    // Allocate space for the tiles and copy them there
    u16 *gfxMain = oamAllocateGfx(&oamMain, SpriteSize_32x32, SpriteColorFormat_256Color);
    dmaCopy(bartTiles, gfxMain, bartTilesLen);

    u16 *gfxSub = oamAllocateGfx(&oamSub, SpriteSize_32x32, SpriteColorFormat_256Color);
    dmaCopy(bartTiles, gfxSub, bartTilesLen);

    // Copy palette
    dmaCopy(bartPal, SPRITE_PALETTE, bartPalLen);
    dmaCopy(bartPal, SPRITE_PALETTE_SUB, bartPalLen);

    int bartCount = 32;
    int currentBartCount = bartCount;

    int bartCoords[2][32];

    for (int i=0;i<bartCount;i++){
        bartCoords[0][i] = rand_int(0,220) + 16;
        bartCoords[1][i] = rand_int(0,170) + 16;

        oamSet(&oamSub, i,
            bartCoords[0][i], bartCoords[1][i], // X, Y
            0, // Priority
            0, // Palette index
            SpriteSize_32x32, SpriteColorFormat_256Color, // Size, format
            gfxSub,  // Graphics offset
            -1, // Affine index
            false, // Double size
            false, // Hide
            false, false, // H flip, V flip
            false); // Mosaic
    }

    bool won = false;
    // Type 1: One bart per click, otherwise multiples Barts per click
    int damageType = 1;

    while (1)
    {
        swiWaitForVBlank();
        bgUpdate();
        scanKeys();

        oamUpdate(&oamMain);
        oamUpdate(&oamSub);

        for (int i=0;i<bartCount;i++){
            int x = bartCoords[0][i];
            int y = bartCoords[1][i];

            if (x == 666) continue;

            int xd = rand_int(0,1);
            int yd = rand_int(0,1);
            int speed = rand_int(0,3);
            if (xd == 0 || x > 240){
                x = x - speed;
            }
            if (xd == 1 || x < 16){
                x = x + speed;
            }
            if (yd == 0 || y > 184){
                y= y - speed;
            }
            if (yd == 1 || y < 16){
                y = y + speed;
            }

            // if (x > 256) x = 4;
            // if (x < 0) x = 250;
            // if (y > 192) y = 4;
            // if (y < 0) y = 180;

            oamSetXY(&oamSub, i, x, y);
            
            bartCoords[0][i] = x;
            bartCoords[1][i] = y;
        }

        uint16_t keys_down = keysDown();

        if (keys_down & KEY_START)
            break;
        
        if (keys_down & KEY_A){
            wavFile = fopen(WAV_FILENAME2, "rb");
            WAVHeader_t wavHeader = { 0 };
            mm_stream stream;
            stream.sampling_rate = wavHeader.sampleRate;
            stream.buffer_length = 16000;
            stream.callback      = streamingCallback;
            stream.format        = getMMStreamType(wavHeader.numChannels, wavHeader.bitsPerSample);
            stream.timer         = MM_TIMER0;
            stream.manual        = false;
            mmStreamOpen(&stream);
        }
        if (keys_down & KEY_B){
            wavFile = fopen(WAV_FILENAME, "rb");
            WAVHeader_t wavHeader = { 0 };
            mm_stream stream;
            stream.sampling_rate = wavHeader.sampleRate;
            stream.buffer_length = 16000;
            stream.callback      = streamingCallback;
            stream.format        = getMMStreamType(wavHeader.numChannels, wavHeader.bitsPerSample);
            stream.timer         = MM_TIMER0;
            stream.manual        = false;
            mmStreamOpen(&stream);
        }
        if (keysDown() & KEY_TOUCH) {
            touchRead(&touch_pos);
            int ld = 16;
            int j;
            for (int i = 0; i < bartCount ; i++){
                //measure distance between click and bart
                double dis = distance(touch_pos.px, touch_pos.py, bartCoords[0][i] + 16, bartCoords[1][i] + 16);
                if (damageType == 1){
                    if (dis < 16) {
                        if (ld > dis){
                            //Find the closest Bart by process of elimination
                            ld = dis;
                            j = i;
                        }
                    }
                }
                else{
                    if (dis < 32) {
                        //Eliminate all clicked barts
                        oamClearSprite(&oamSub, i);
                        mmEffect(SFX_OW);
                        currentBartCount--;
                        bartCoords[0][i] = 666; bartCoords[1][i] = 666;
                    }
                }
            }
            if (damageType == 1 && ld < 16){
                //Eliminate the closest bart clicked
                oamClearSprite(&oamSub, j);
                mmEffect(SFX_OW);
                currentBartCount--;
                bartCoords[0][j] = 666; bartCoords[1][j] = 666;
            }
        }

        if (currentBartCount == 0 && won == false){
            printf("You're winner!");
            bgMosaicDisable(2);
            dmaCopy(congratTiles, bgGetGfxPtr(bg), congratTilesLen);
            dmaCopy(congratMap, bgGetMapPtr(bg), congratMapLen);
            dmaCopy(congratPal, BG_PALETTE, congratPalLen);
            mmStreamClose();
            mmEffect(SFX_CONGRAT);
            won = true;
        }
    }


    oamFreeGfx(&oamMain, gfxMain);
    oamFreeGfx(&oamSub, gfxSub);
    mmStreamClose();

    if (fclose(wavFile) != 0) {
        perror("fclose");
        waitForever();
    }

    soundDisable();

    return 0;
}
