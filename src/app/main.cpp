#include "app.h"

int WINAPI wWinMain(HINSTANCE instanceHandle, HINSTANCE, PWSTR commandLine, int) {
    // Parse command line manually (wWinMain doesn't provide argc/argv)
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(commandLine, &argc);

    goldview::App app(instanceHandle);
    const bool initialized = app.initialize(argc, argv);

    if (argv) {
        LocalFree(argv);
    }

    if (!initialized) {
        return 0;  // Silent exit (e.g., skipped on weekends)
    }
    return app.run();
}
