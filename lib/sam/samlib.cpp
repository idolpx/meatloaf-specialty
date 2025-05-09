
#include "samlib.h"

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <driver/gpio.h>
#if !defined(CONFIG_IDF_TARGET_ESP32S3) & !defined(CONFIG_IDF_TARGET_ESP32C3)
#include <driver/dac.h>
#endif


#include "fnSystem.h"

#ifdef __cplusplus
extern char input[256];
extern char *buffer;
#endif

int debug = 0;

#ifndef ESP_PLATFORM

void WriteWav(char *filename, char *buffer, int bufferlength)
{
    FILE *file = fopen(filename, "wb");
    if (file == NULL)
        return;
    //RIFF header
    fwrite("RIFF", 4, 1, file);
    unsigned int filesize = bufferlength + 12 + 16 + 8 - 8;
    fwrite(&filesize, 4, 1, file);
    fwrite("WAVE", 4, 1, file);

    //format chunk
    fwrite("fmt ", 4, 1, file);
    unsigned int fmtlength = 16;
    fwrite(&fmtlength, 4, 1, file);
    unsigned short int format = 1; //PCM
    fwrite(&format, 2, 1, file);
    unsigned short int channels = 1;
    fwrite(&channels, 2, 1, file);
    unsigned int samplerate = 22050;
    fwrite(&samplerate, 4, 1, file);
    fwrite(&samplerate, 4, 1, file); // bytes/second
    unsigned short int blockalign = 1;
    fwrite(&blockalign, 2, 1, file);
    unsigned short int bitspersample = 8;
    fwrite(&bitspersample, 2, 1, file);

    //data chunk
    fwrite("data", 4, 1, file);
    fwrite(&bufferlength, 4, 1, file);
    fwrite(buffer, bufferlength, 1, file);

    fclose(file);
}
#endif // NOT ESP_PLATFORM

void PrintUsage()
{
    /*
    printf("\r\n");
    printf("Usage: sam [options] Word1 Word2 ....\r\n");
    printf("options\r\n");
    printf("    -phonetic         enters phonetic mode. (see below)\r\n");
    printf("    -pitch number        set pitch value (default=64)\r\n");
    printf("    -speed number        set speed value (default=72)\r\n");
    printf("    -throat number        set throat value (default=128)\r\n");
    printf("    -mouth number        set mouth value (default=128)\r\n");
    printf("    -wav filename        output to wav instead of libsdl\r\n");
    printf("    -sing            special treatment of pitch\r\n");
    printf("    -debug            print additional debug messages\r\n");
    printf("\r\n");

    printf("     VOWELS                            VOICED CONSONANTS    \r\n");
    printf("IY           f(ee)t                    R        red        \r\n");
    printf("IH           p(i)n                     L        allow        \r\n");
    printf("EH           beg                       W        away        \r\n");
    printf("AE           Sam                       W        whale        \r\n");
    printf("AA           pot                       Y        you        \r\n");
    printf("AH           b(u)dget                  M        Sam        \r\n");
    printf("AO           t(al)k                    N        man        \r\n");
    printf("OH           cone                      NX       so(ng)        \r\n");
    printf("UH           book                      B        bad        \r\n");
    printf("UX           l(oo)t                    D        dog        \r\n");
    printf("ER           bird                      G        again        \r\n");
    printf("AX           gall(o)n                  J        judge        \r\n");
    printf("IX           dig(i)t                   Z        zoo        \r\n");
    printf("                       ZH       plea(s)ure    \r\n");
    printf("   DIPHTHONGS                          V        seven        \r\n");
    printf("EY           m(a)de                    DH       (th)en        \r\n");
    printf("AY           h(igh)                        \r\n");
    printf("OY           boy                        \r\n");
    printf("AW           h(ow)                     UNVOICED CONSONANTS    \r\n");
    printf("OW           slow                      S         Sam        \r\n");
    printf("UW           crew                      Sh        fish        \r\n");
    printf("                                       F         fish        \r\n");
    printf("                                       TH        thin        \r\n");
    printf(" SPECIAL PHONEMES                      P         poke        \r\n");
    printf("UL           sett(le) (=AXL)           T         talk        \r\n");
    printf("UM           astron(omy) (=AXM)        K         cake        \r\n");
    printf("UN           functi(on) (=AXN)         CH        speech        \r\n");
    printf("Q            kitt-en (glottal stop)    /H        a(h)ead    \r\n");
    */
}

#ifdef USESDL

int pos = 0;
void MixAudio(void *unused, Uint8 *stream, int len)
{
    int bufferpos = GetBufferLength();
    char *buffer = GetBuffer();
    int i;
    if (pos >= bufferpos)
        return;
    if ((bufferpos - pos) < len)
        len = (bufferpos - pos);
    for (i = 0; i < len; i++)
    {
        stream[i] = buffer[pos];
        pos++;
    }
}

void OutputSound()
{
    int bufferpos = GetBufferLength();
    bufferpos /= 50;
    SDL_AudioSpec fmt;

    fmt.freq = 22050;
    fmt.format = AUDIO_U8;
    fmt.channels = 1;
    fmt.samples = 2048;
    fmt.callback = MixAudio;
    fmt.userdata = NULL;

    /* Open the audio device and start playing sound! */
    if (SDL_OpenAudio(&fmt, NULL) < 0)
    {
        printf("Unable to open audio: %s\r\n", SDL_GetError());
        exit(1);
    }
    SDL_PauseAudio(0);
    //SDL_Delay((bufferpos)/7);

    while (pos < bufferpos)
    {
        SDL_Delay(100);
    }

    SDL_CloseAudio();
}

#else

void OutputSound()
{
#ifdef ESP_PLATFORM
#if !defined(CONFIG_IDF_TARGET_ESP32S3) & !defined(CONFIG_IDF_TARGET_ESP32C3)
    int n = GetBufferLength() / 50;
    char *s = GetBuffer();

    //fnSystem.dac_output_enable(SystemManager::dac_channel_t::DAC_CHAN_0);
    //fnSystem.dac_output_voltage(SystemManager::dac_channel_t::DAC_CHAN_0, 100);

    dac_output_enable(DAC_CHANNEL_1);

    for (int i = 0; i < n; i++)
    {
        //dacWrite(DAC1, s[i]);
        // fnSystem.dac_write(PIN_DAC1, s[i]);
        dac_output_voltage(DAC_CHANNEL_1, s[i]);
        //delayMicroseconds(40);
        fnSystem.delay_microseconds(40);
    }

    //fnSystem.dac_output_disable(SystemManager::dac_channel_t::DAC_CHANNEL_1);
    dac_output_disable(DAC_CHANNEL_1);

    FreeBuffer();
#endif
#endif
}

#endif

int sam(int argc, char **argv)
{
    int i;
    int phonetic = 0;

#ifndef ESP_PLATFORM
    char *wavfilename = NULL;
#endif

    for (i = 0; i < 256; i++)
        input[i] = 0;

    if (argc <= 1)
    {
        PrintUsage();
        return 1;
    }

    i = 1;
    while (i < argc)
    {
        if (argv[i][0] != '-')
        {
            strlcat(input, argv[i], 255);
            strlcat(input, " ", 255);
        }
        else
        {

            if (strcmp(&argv[i][1], "wav") == 0)
            {
#ifndef ESP_PLATFORM
                wavfilename = argv[i + 1];
#endif
                i++;
            }
            else

                if (strcmp(&argv[i][1], "sing") == 0)
            {
                EnableSingmode();
            }
            else if (strcmp(&argv[i][1], "phonetic") == 0)
            {
                phonetic = 1;
            }
            else if (strcmp(&argv[i][1], "debug") == 0)
            {
                debug = 1;
            }
            else if (strcmp(&argv[i][1], "pitch") == 0)
            {
                SetPitch(atoi(argv[i + 1]));
                i++;
            }
            else if (strcmp(&argv[i][1], "speed") == 0)
            {
                SetSpeed(atoi(argv[i + 1]));
                i++;
            }
            else if (strcmp(&argv[i][1], "mouth") == 0)
            {
                SetMouth(atoi(argv[i + 1]));
                i++;
            }
            else if (strcmp(&argv[i][1], "throat") == 0)
            {
                SetThroat(atoi(argv[i + 1]));
                i++;
            }
            else
            {
                PrintUsage();
                return 1;
            }
        }

        i++;
    } //while

    // printf("arg parsing done\r\n");

    for (i = 0; input[i] != 0; i++)
        input[i] = toupper((int)input[i]);

    if (debug)
    {
        if (phonetic)
            printf("phonetic input: %s\r\n", input);
        else
            printf("text input: %s\r\n", input);
    }

    if (!phonetic)
    {
        strlcat(input, "[", sizeof(input) - strlen(input));
        // printf("TextToPhonemes\r\n");
        if (!TextToPhonemes((unsigned char *)input))
            return 1;
        if (debug)
            printf("phonetic input: %s\r\n", input);
    }
    else
        strlcat(input, "\x9b", sizeof(input) - strlen(input));

        // printf("done phonetic processing\r\n");

#ifdef USESDL
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        printf("Unable to init SDL: %s\r\n", SDL_GetError());
        exit(1);
    }
    atexit(SDL_Quit);
#endif

#ifndef __cplusplus
    SetInput(input);
#endif

    // printf("right before SAMMain");

    if (!SAMMain())
    {
        PrintUsage();
        return 1;
    }
    // printf("right after SAMMain");

#ifndef ESP_PLATFORM
    if (wavfilename != NULL)
        WriteWav(wavfilename, GetBuffer(), GetBufferLength() / 50);
    else
#endif // ESP_PLATFORM
        OutputSound();

    return 0;
}
