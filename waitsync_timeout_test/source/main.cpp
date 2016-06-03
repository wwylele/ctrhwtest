#include <3ds.h>
#include <stdio.h>

const double TICKS_PER_SEC = 268123480;
const u64 ONE_SEC = 1000 * 1000 * 1000;
u64 begin;

double TimeElapseFromBegin() {
    return (svcGetSystemTick() - begin) / TICKS_PER_SEC;
}

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

Handle objects[2], threadA;
Handle& threadB = objects[1];

u32 threadA_stack[0x400];
u32 threadB_stack[0x400];

void threadA_entry(void*) {
    s32 output;
    svcWaitSynchronizationN(&output, objects, 2, true, 3 * ONE_SEC);
    svcExitThread();
}
void threadB_entry(void*) {
    svcSleepThread(1 * ONE_SEC);
    svcExitThread();
}
void TestWaitN() {
    printf("\n--svcWaitSynchronizationN timeout test--\n");
    printf("Thread A and B are going to start.\n");
    printf("A will WaitSynchronizationN on both B and a dummy event. Timout = 3 sec.\n");
    printf("B will sleep for 1 sec and exit.\n");
    printf("Press A to start\n");
    Pause();
    printf("RUN!>>>\n");

    svcCreateEvent(&objects[0], 1); // a dummy event that will never be signaled'

    svcCreateThread(&threadA, threadA_entry, 0x0, threadA_stack + 0x400, 0x31, 0xFFFFFFFE);
    svcCreateThread(&threadB, threadB_entry, 0x0, threadB_stack + 0x400, 0x31, 0xFFFFFFFE);

    begin = svcGetSystemTick();
    svcWaitSynchronization(threadB, U64_MAX);
    printf("Thread B exits. time = %.1f\n", TimeElapseFromBegin());
    svcWaitSynchronization(threadA, U64_MAX);
    printf("Thread A exits. time = %.1f\n", TimeElapseFromBegin());
    svcCloseHandle(objects[0]);
    svcCloseHandle(threadA);
    svcCloseHandle(threadB);
}

/////////////////////////////////////////////////////////////////////////////////

Handle mutex;
Handle getMutexEvent, timeoutEvent;
u32 threadC_stack[0x400];
u32 threadD_stack[0x400];
void threadCD_entry(void*) {
    if (svcWaitSynchronization(mutex, 3 * ONE_SEC) == 0x09401BFE/*timeout*/) {
        svcSignalEvent(timeoutEvent);
    } else {
        svcSignalEvent(getMutexEvent);
        svcSleepThread(4 * ONE_SEC);
        svcReleaseMutex(mutex);
    }
    svcExitThread();
}

void TestWait1() {
    printf("\n--svcWaitSynchronization1 timeout test--\n");
    printf("Two threads are going to start.\n");
    printf("both of them will WaitSynchronization1 on a mutex. Timeout = 3 sec.\n");
    printf("The one which gets the mutex will hold it for 4 sec.\n");
    printf("The main thread will hold the mutex first for 1 sec.\n");
    printf("Press A to start\n");
    Pause();
    printf("RUN!>>>\n");

    Handle threads[2];
    svcCreateMutex(&mutex, true);
    svcCreateEvent(&getMutexEvent, 1);
    svcCreateEvent(&timeoutEvent, 1);
    svcCreateThread(&threads[0], threadCD_entry, 0x0, threadC_stack + 0x400, 0x31, 0xFFFFFFFE);
    svcCreateThread(&threads[1], threadCD_entry, 0x0, threadD_stack + 0x400, 0x31, 0xFFFFFFFE);

    begin = svcGetSystemTick();
    svcSleepThread(ONE_SEC);
    svcReleaseMutex(mutex);
    svcWaitSynchronization(getMutexEvent, U64_MAX);
    printf("One thread gets the mutex. time = %.1f\n", TimeElapseFromBegin());
    svcWaitSynchronization(timeoutEvent, U64_MAX);
    printf("The other thread times out. time = %.1f\n", TimeElapseFromBegin());

    s32 output;
    svcWaitSynchronizationN(&output, threads, 2, true, U64_MAX);
    printf("Both theads exit. time = %.1f\n", TimeElapseFromBegin());
    svcCloseHandle(threads[0]);
    svcCloseHandle(threads[1]);
    svcCloseHandle(mutex);
    svcCloseHandle(getMutexEvent);
    svcCloseHandle(timeoutEvent);
}
/////////////////////////////////////////////////////////////////////////////////



int main(int argc, char **argv) {
    gfxInitDefault();

    consoleInit(GFX_TOP, NULL);

    TestWaitN();

    printf("Press A to continue\n");
    Pause();

    TestWait1();

    printf("Press A to exit\n");
    Pause();

    gfxExit();
    return 0;
}
