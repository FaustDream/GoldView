#include "app.h"

#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE instanceHandle, HINSTANCE, PWSTR commandLine, int) {
    // 手动解析命令行参数，wWinMain 不会直接提供 argc/argv。
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(commandLine, &argc);

    goldview::App app(instanceHandle);
    const bool initialized = app.initialize(argc, argv);

    if (argv) {
        LocalFree(argv);
    }

    if (!initialized) {
        return 0;
    }
    return app.run();
}
