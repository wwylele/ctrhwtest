#include <string>
#include <vector>
#include <3ds.h>
#include <stdio.h>

Handle hidEvents[5];

int TestEventFrequency(Handle event) {
    Handle timer;
    svcCreateTimer(&timer, RESET_ONESHOT);
    Handle waiting_list[2]{timer, event};
    svcSetTimer(timer, 1000000000, 0);
    int f = 0;
    s32 wait_index = 0;
    while (1) {
        svcWaitSynchronizationN(&wait_index, waiting_list, 2, false, 2000000000);
        if (wait_index == 0)
            break;
        f++;
        // svcClearEvent(event);
    }
    svcCloseHandle(timer);
    return f;
}

int main() {
    aptInit();
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    hidInit();

    printf("GetHandle:0x%08lX\n", HIDUSER_GetHandles(0, &hidEvents[0], &hidEvents[1], &hidEvents[2],
                                                     &hidEvents[3], &hidEvents[4]));

    u32 kDown;
    int i = 0;
    bool accel = false, gyro = false;
    while (aptMainLoop()) {
        hidScanInput();
        kDown = hidKeysDown();

        if (kDown & KEY_START) {
            break;
        } else if (kDown & KEY_A) {
            printf("Testing frequency of event %d...\n", i);
            int f = TestEventFrequency(hidEvents[i]);
            printf("result = %d\n", f);

            ++i;
            if (i == 5)
                i = 0;
        } else if (kDown & KEY_X) {
            if (accel) {
                printf("HIDUSER_DisableAccelerometer:0x%08lX\n", HIDUSER_DisableAccelerometer());
                accel = false;
            } else {
                printf("HIDUSER_EnableAccelerometer:0x%08lX\n", HIDUSER_EnableAccelerometer());
                accel = true;
            }
        } else if (kDown & KEY_Y) {
            if (gyro) {
                printf("HIDUSER_DisableGyroscope:0x%08lX\n", HIDUSER_DisableGyroscope());
                gyro = false;
            } else {
                printf("HIDUSER_EnableGyroscope:0x%08lX\n", HIDUSER_EnableGyroscope());
                gyro = true;
            }
        }

        gfxFlushBuffers();
        gspWaitForVBlank();
        gfxSwapBuffers();
    }

    hidExit();

    gfxExit();
    return 0;
}
