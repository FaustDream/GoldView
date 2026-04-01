#include "app.h"

int WINAPI wWinMain(HINSTANCE instanceHandle, HINSTANCE, PWSTR, int) {
    goldview::App app(instanceHandle);
    if (!app.initialize()) {
        return 1;
    }
    return app.run();
}
