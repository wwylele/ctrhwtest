#include <3ds.h>
#include <stdio.h>


const u64 ONE_SEC = 1000 * 1000 * 1000;

void Pause() {
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_A) break;

        gfxFlushBuffers();
        gfxSwapBuffers();

        gspWaitForVBlank();
    }
}

/////////////////////////////////////////////////////////////////////////////////

Handle threadAB[2];
u32 threadA_stack[0x400];
u32 threadB_stack[0x400];
Handle X; // will be an event or timer
u32 counter;
void threadAB_entry(void*) {
    if (0 == svcWaitSynchronization(X, ONE_SEC * 2)) {
        AtomicIncrement(&counter);
    }
    svcExitThread();
}

void TestPower(bool timer, u8 type) {
    const char* object_desc = timer ? "timer" : "event";
    puts(">>>>>> press A to start <<<<<<");
    printf("%s power test. type = %d\n", object_desc, type);
    Pause();
    if (timer)
        svcCreateTimer(&X, type);
    else
        svcCreateEvent(&X, type);

    counter = 0;
    svcCreateThread(&threadAB[0], threadAB_entry, 0x0, threadA_stack + 0x400, 0x31, 0xFFFFFFFE);
    svcCreateThread(&threadAB[1], threadAB_entry, 0x0, threadB_stack + 0x400, 0x31, 0xFFFFFFFE);

    if (timer) {
        svcSetTimer(X, ONE_SEC, 0);
    } else {
        svcSleepThread(ONE_SEC);
        svcSignalEvent(X);
    }
    

    s32 output;
    svcWaitSynchronizationN(&output, threadAB, 2, true, U64_MAX);
    if (counter == 1) {
        puts("Only one thread wakes up!");
    } else if (counter == 2) {
        puts("Both threads wake up!");
    }

    if (0 == svcWaitSynchronization(X, 1)) {
        printf("The %s is not cleared!\n", object_desc);
    } else {
        printf("The %s is cleared!\n", object_desc);
    }

    svcCloseHandle(X);
    svcCloseHandle(threadAB[0]);
    svcCloseHandle(threadAB[1]);
        
    puts("------------------");
    Pause();
}

/////////////////////////////////////////////////////////////////////////////////
Handle threadC;
u32 threadC_stack[0x400];
Handle Y; // will be an event or timer

void threadC_entry(void*) {
    svcSleepThread(ONE_SEC);
    svcSignalEvent(Y);
    svcExitThread();
}

void TestMissing(bool timer, u8 type) {
    const char* object_desc = timer ? "timer" : "event";
    puts(">>>>>> press A to start <<<<<<");
    printf("%s missing test. type = %d\n", object_desc, type);
    Pause();

    if (timer) {
        svcCreateTimer(&Y, type);
        svcSetTimer(Y, ONE_SEC, 0);
    } else {
        svcCreateEvent(&Y, type);
        svcCreateThread(&threadC, threadC_entry, 0x0, threadC_stack + 0x400, 0x31, 0xFFFFFFFE);
    }
    
    svcSleepThread(ONE_SEC * 2);

    for (int i = 0; i < 2; ++i) if (0 == svcWaitSynchronization(Y, 1)) {
        printf("The %s is not cleared!\n", object_desc);
    } else {
        printf("The %s is cleared!\n", object_desc);
    }

    if (!timer) {
        svcWaitSynchronization(threadC, U64_MAX);
        svcCloseHandle(threadC);
    }
    svcCloseHandle(Y);
    

    puts("------------------");
    Pause();
}


/////////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv) {
    gfxInitDefault();

    consoleInit(GFX_TOP, NULL);

    puts("Power test:\nTwo threads wait on a event/timer,");
    puts("which will be signaled in 1 sec.");
    puts("We will check if both of them wake up,");
    puts("and check if the event/timer is still signaled");
    for (u8 t = 0; t < 3; ++t) {
        TestPower(false, t);
    }
    
    for (u8 t = 0; t < 3; ++t) {
        TestPower(true, t);
    }

    puts("Missing test:\n A event/timer will be signaled in 1 sec,");
    puts("while the thread will sleep for 2 sec first,");
    puts("then try waiting on the event/timer twice.");
    for (u8 t = 0; t < 3; ++t) {
        TestMissing(false, t);
    }
    for (u8 t = 0; t < 3; ++t) {
        TestMissing(true, t);
    }

    printf("Press A to exit\n");
    Pause();

    gfxExit();
    return 0;
}
