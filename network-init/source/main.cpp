#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void Pause() {
    puts("Press A to continue.");
    // Main loop
    while (aptMainLoop())
    {
        hidScanInput();

        // Respond to user input
        u32 kDown = hidKeysDown();
        if (kDown & KEY_A)
            break; // break in order to return to hbmenu
    }
}

u32 response[0x40];

void GetResponse() {
    u32* cmd_buff = getThreadCommandBuffer();
    memcpy(response, cmd_buff, 0x100);
}

const int shmem_size = 0x14000;
Handle nwm;
Handle shmem;

void testNWMInit() {
    u32* cmd_buff = getThreadCommandBuffer();
    cmd_buff[0]=0x001B0302,
    /// parameters are taken from Pokemon OR/AS
    cmd_buff[1]=shmem_size, // shared memory size
    cmd_buff[2]=0x3C05EBB,
    cmd_buff[3]=0x800F0000,
    cmd_buff[4]=0x770077,
    cmd_buff[5]=0x6C0079,
    cmd_buff[6]=0x6C,
    cmd_buff[7]=0x0,
    cmd_buff[8]=0x0,
    cmd_buff[9]=0x0,
    cmd_buff[10]=0x0,
    cmd_buff[11]=0x0,
    cmd_buff[12]=0x400, //version
    ///
    cmd_buff[13]=0x0, //handle translation
    cmd_buff[14]=shmem; //shared memory

    u32 result = (u32)svcSendSyncRequest(nwm);
    GetResponse();

    printf("%08lX from svcSendSyncRequest\n", result);

    printf("%08lX from nwmInit[0]\n", response[0]);
    printf("%08lX from nwmInit[1]\n", response[1]);
    printf("%08lX from nwmInit[2]\n", response[2]);
    printf("%08lX from nwmInit[3]\n", response[3]);

}

int main()
{
    gfxInitDefault();

    consoleInit(GFX_BOTTOM, NULL);

    void* shmem_m = malloc(shmem_size);

    printf("%08lX from CreateMemoryBlock\n", (u32)svcCreateMemoryBlock(
        &shmem, (u32)shmem_m, shmem_size, (MemPerm)0, (MemPerm)3
    ));

    printf("%08lX from srvGetServiceHandle(nwm)\n", (u32)srvGetServiceHandle(
        &nwm, "nwm::UDS"
    ));

    testNWMInit();

    Pause();

	gfxExit();

    return 0;
}
