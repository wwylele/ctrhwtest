#include <3ds.h>
#include <stdio.h>

const s64 ONE_SEC = 1000*1000*1000;
const Result TIMEOUT = 0x09401BFE;

void Pause() {
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_A) break;

        gfxFlushBuffers();
        gfxSwapBuffers();

        gspWaitForVBlank();
    }
}

s32 GetSemaCount(Handle sema) {
    s32 count;
    svcReleaseSemaphore(&count, sema, 0);
    return count;
}

int main(int argc, char **argv) {
    gfxInitDefault();

    consoleInit(GFX_TOP, NULL);

    Result result;

    Handle sema;
    svcCreateSemaphore(&sema, 7, 7);
    printf("available_count=%ld\n", GetSemaCount(sema));

    puts("acquire via WaitSynch1");
    result = svcWaitSynchronization(sema, ONE_SEC);
    if (result == TIMEOUT) puts("Timeout!");
    printf("available_count=%ld\n", GetSemaCount(sema));

    Handle semas[] = {sema, sema, sema};
    s32 out;

    puts("acquire via WaitSynchN, N=3, wait_all=true");
    result = svcWaitSynchronizationN(&out, semas, 3, true, ONE_SEC);
    if (result == TIMEOUT) puts("Timeout!");
    printf("available_count=%ld\n", GetSemaCount(sema));

    puts("acquire via WaitSynchN, N=3, wait_all=false");
    result = svcWaitSynchronizationN(&out, semas, 3, false, ONE_SEC);
    if (result == TIMEOUT) puts("Timeout!");
    printf("available_count=%ld\n", GetSemaCount(sema));

    puts("acquire via WaitSynchN, N=3, wait_all=true");
    result = svcWaitSynchronizationN(&out, semas, 3, true, ONE_SEC);
    if (result == TIMEOUT) puts("Timeout!");
    printf("available_count=%ld\n", GetSemaCount(sema));



    printf("Press A to exit\n");
    Pause();

    gfxExit();
    return 0;
}
