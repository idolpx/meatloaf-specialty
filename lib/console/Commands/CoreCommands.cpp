#include "./CoreCommands.h"
#include "linenoise/linenoise.h"
#include "soc/soc_caps.h"
//#include "argparse/argparse.hpp"

#include <string>

#include "string_utils.h"

static int clear(int argc, char **argv)
{
    // If we are on a dumb erminal clearing does not work
    if (linenoiseProbe())
    {
        printf("\r\nYour terminal does not support escape sequences. Clearing screen does not work!\r\n");
        return EXIT_FAILURE;
    }

    linenoiseClearScreen();
    return EXIT_SUCCESS;
}

static int echo(int argc, char **argv)
{
    for (int n = 1; n<argc; n++)
    {
        printf("%s ", argv[n]);
    }
    printf("\r\n");

    return EXIT_SUCCESS;
}

static int set_multiline_mode(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("You have to give 'on' or 'off' as an argument!\r\n");
        return EXIT_FAILURE;
    }

    // Get argument
    auto mode = std::string(argv[1]);
    // Normalize
    mstr::toLower(mode);

    if (mode == "on")
    {
        linenoiseSetMultiLine(1);
    }
    else if (mode == "off")
    {
        linenoiseSetMultiLine(0);
    }
    else
    {
        printf("Unknown option. Pass 'on' or 'off' (without quotes)!\r\n");
        return EXIT_FAILURE;
    }

    printf("Multiline mode set.\r\n");

    return EXIT_SUCCESS;
}

static int history_channel = 0;

static int history(int argc, char **argv)
{
    // If arguments were passed check for clearing
    /*if (argc > 1)
    {
        if (strcasecmp(argv[1], "-c"))
        { // When -c option was detected clear history.
            linenoiseHistorySetMaxLen(0);
            printf("History cleared!\r\n");
            linenoiseHistorySetMaxLen(10);
            return EXIT_SUCCESS;
        }
        else
        {
            printf("Invalid argument. Use -c to clear history.\r\n");

            return EXIT_FAILURE;
        }
    }
    else*/
    { // Without arguments we just output the history
      // We use the ESP-IDF VFS to directly output the file to an UART. UART channel 0 has the path /dev/uart/0 and so on.
        char path[12] = {0};
        snprintf(path, 12, "/dev/uart/%d", history_channel);

        // If we found the correct one, let linoise save (output) them.
        linenoiseHistorySave(path);
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}

extern char **environ;

static int env(int argc, char **argv)
{
    char **s = environ;

    for (; *s; s++)
    {
        printf("%s\r\n", *s);
    }
    return EXIT_SUCCESS;
}

static int declare(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Syntax: declare VAR short OR declare VARIABLE \"Long Value\"\r\n");
        return EXIT_FAILURE; 
    }

    setenv(argv[1], argv[2], 1);

    return EXIT_SUCCESS;
}

namespace ESP32Console::Commands
{
    const ConsoleCommand getClearCommand()
    {
        return ConsoleCommand("clear", &clear, "Clears the screen using ANSI codes");
    }

    const ConsoleCommand getEchoCommand()
    {
        return ConsoleCommand("echo", &echo, "Echos the text supplied as argument");
    }

    const ConsoleCommand getSetMultilineCommand()
    {
        return ConsoleCommand("multiline_mode", &set_multiline_mode, "Sets the multiline mode of the console");
    }

    const ConsoleCommand getHistoryCommand(int uart_channel)
    {
        history_channel = uart_channel;
        return ConsoleCommand("history", &history, "Shows and clear command history (using -c parameter)");
    }

    const ConsoleCommand getEnvCommand()
    {
        return ConsoleCommand("env", &env, "List all environment variables.");
    }

    const ConsoleCommand getDeclareCommand()
    {
        return ConsoleCommand("declare", &declare, "Change enviroment variables");
    }
}