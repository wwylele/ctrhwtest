#include <3ds.h>
#include <stdio.h>


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

    Handle sema;
    svcCreateSemaphore(&sema, 10, 10);
    printf("available_count=%ld\n", GetSemaCount(sema));

    puts("acquire via WaitSynch1");
    svcWaitSynchronization(sema, U64_MAX);
    printf("available_count=%ld\n", GetSemaCount(sema));

    Handle semas[] = {sema, sema, sema};
    s32 out;

    puts("acquire via WaitSynchN, N=3, wait_all=true");
    svcWaitSynchronizationN(&out, semas, 3, true, U64_MAX);
    printf("available_count=%ld\n", GetSemaCount(sema));

    puts("acquire via WaitSynchN, N=3, wait_all=false");
    svcWaitSynchronizationN(&out, semas, 3, false, U64_MAX);
    printf("available_count=%ld\n", GetSemaCount(sema));


    printf("Press A to exit\n");
    Pause();

    gfxExit();
    return 0;
}
