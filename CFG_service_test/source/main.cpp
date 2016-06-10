#include <3ds.h>
#include <stdio.h>
#include <string.h>

// We want to access cfg:s service. So service patch may be required.
// This is wwylele's util. Please replace it with yours or remove it
#include <service_patch.h>

Handle cfgsHandle;

u32 cmdbuf[0x40];

void SendCommandBuffer(){
    memcpy(getThreadCommandBuffer(), cmdbuf, 0x100);
}

void RecieveCommandBuffer(){
    memcpy(cmdbuf, getThreadCommandBuffer(), 0x100);
}

void TestSetConfigInfo4(u32 size, u32 blkID, u8* data) {
    cmdbuf[0] = IPC_MakeHeader(0x402, 2, 2); // SetConfigBlk4
    //Switch the parameters!
    cmdbuf[1] = blkID;
    cmdbuf[2] = size;
    cmdbuf[3] = IPC_Desc_Buffer(size, IPC_BUFFER_R);
    cmdbuf[4] = (u32)data;

    SendCommandBuffer();
    if (R_FAILED(svcSendSyncRequest(cfgsHandle))) {
        printf("SendSyncRequest fialed\n\n");
    } else {
        RecieveCommandBuffer();
        printf("result = %08X\n\n", cmdbuf[1]);
    }
}

void TestGetConfigInfo(bool eight, u32 size, u32 blkID, u8* data) {
    cmdbuf[0] = IPC_MakeHeader(eight ? 0x401:0x001, 2, 2); // SetConfigBlk8/2
    cmdbuf[1] = size;
    cmdbuf[2] = blkID;
    cmdbuf[3] = IPC_Desc_Buffer(size, IPC_BUFFER_W);
    cmdbuf[4] = (u32)data;

    SendCommandBuffer();
    if (R_FAILED(svcSendSyncRequest(cfgsHandle))) {
        printf("SendSyncRequest fialed\n");
    } else {
        RecieveCommandBuffer();
        printf("result = %08X\n", cmdbuf[1]);
    }
}

void Pause() {
    puts("Press A to continue");
    while (aptMainLoop()) {
        hidScanInput();

        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        if (kDown & KEY_A) break;

        gfxFlushBuffers();
        gfxSwapBuffers();

        gspWaitForVBlank();
    }
}

int main(int argc, char **argv) {
    gfxInitDefault();

    consoleInit(GFX_TOP, NULL);

    PatchServiceAccess(); // We want to access cfg:s service. So service patch may be required.

    if (R_FAILED(srvGetServiceHandle(&cfgsHandle, "cfg:s"))) {
        printf("get cfg:s service handle failed\n");
    } else {
        u32 id = 0x000A0001; // birthday
        u8 data[] = {12, 25}; // wwylele's birthday + 1
        u8 data2[2];

        puts("\nSetConfigInfoBlk4 Test case 0 - normal");
        TestSetConfigInfo4(2, id, data);

        puts("\nSetConfigInfoBlk4 Test case 1 - bad size");
        TestSetConfigInfo4(1, id, data);

        puts("\nSetConfigInfoBlk4 Test case 2 - bad id");
        TestSetConfigInfo4(2, 0x00000001, data); // this block doesn't exist on wwylele's 3DS

        Pause();

        puts("\nGetConfigInfoBlk8 Test case 0 - normal");
        TestGetConfigInfo(true, 2, id, data2);
        printf("block content = %d,%d\n", data2[0], data2[1]);

        puts("\nGetConfigInfoBlk8 Test case 1 - (bad) flag");
        TestGetConfigInfo(true, 2, 0x00000000, data2); // this block id is valid but with flag 0xC on wwylele's 3DS
        printf("block content = %d,%d\n", data2[0], data2[1]);

        puts("\nGetConfigInfoBlk8 Test case 2 - bad size");
        TestGetConfigInfo(true, 1, id, data2);

        puts("\nGetConfigInfoBlk8 Test case 3 - bad id");
        TestGetConfigInfo(true, 2, 0x00000001, data2);

        Pause();

        puts("\nGetConfigInfoBlk2 Test case 0 - normal");
        TestGetConfigInfo(false, 2, id, data2);
        printf("block content = %d,%d\n", data2[0], data2[1]);

        puts("\nGetConfigInfoBlk2 Test case 1 - bad flag");
        TestGetConfigInfo(false, 2, 0x00000000, data2);

        puts("\nGetConfigInfoBlk2 Test case 2 - bad size");
        TestGetConfigInfo(false, 1, id, data2);

        puts("\nGetConfigInfoBlk2 Test case 3 - bad id");
        TestGetConfigInfo(false, 2, 0x00000001, data2);

        puts("\nGetConfigInfoBlk2 Test case 4 - bad flag + bad size");
        TestGetConfigInfo(false, 1, 0x00000000, data2);

        puts("Done!");
        Pause();
    }

    gfxExit();
    return 0;
}
