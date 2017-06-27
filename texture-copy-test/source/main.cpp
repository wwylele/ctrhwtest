#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

std::string GetCommand() {
    static SwkbdState swkbd;
	static char mybuf[60];

	swkbdInit(&swkbd, SWKBD_TYPE_WESTERN, 2, -1);
	swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
	swkbdSetHintText(&swkbd, "Please input a command.");

	if (SWKBD_BUTTON_RIGHT != swkbdInputText(&swkbd, mybuf, sizeof(mybuf)))
        return "";

    return mybuf;
}
 
const unsigned BUFFER_SIZE = 16 * 16 * 16;
unsigned char *A, *B;

void Test(unsigned widthA, unsigned gapA, unsigned widthB, unsigned gapB, unsigned length) {
    consoleClear();
    printf("(%u, %u)->(%u, %u), len=%u (0x%X blocks)\n", widthA, gapA, widthB, gapB, length, length / 16);
    for (unsigned i = 0; i < BUFFER_SIZE; ++i) {
        B[i] = 0;
    }
    GSPGPU_FlushDataCache(B, BUFFER_SIZE);
    GX_TextureCopy((u32*)A, GX_BUFFER_DIM(widthA, gapA) ,(u32*)B ,GX_BUFFER_DIM(widthB, gapB) ,length, 8);
    gspWaitForPPF();
    GSPGPU_FlushDataCache(B, BUFFER_SIZE);
    
    bool prev_strange = false;
    for (unsigned i = 0; i < BUFFER_SIZE; ++i) {
        unsigned j = i / 16;
        
        if (i % 16 == 0) {
            if (j % 16 == 0) {
                printf("\n");
            }
            if (prev_strange) {
                printf("?");
            } else {
                printf(" ");
            }
            printf("%02X", B[i]);
            prev_strange = false;
        } else {
            if (B[i] != B[j * 16])
                prev_strange = true;
        }
    }
    
}
 

	
int main()
{
    gfxInitDefault();

    consoleInit(GFX_TOP, NULL);
    
    A = (unsigned char *)linearAlloc(BUFFER_SIZE);
    B = (unsigned char *)linearAlloc(BUFFER_SIZE);
    
    for (unsigned i = 0; i < BUFFER_SIZE; ++i) {
        A[i] = (i / 16) ;
    }
    
    GSPGPU_FlushDataCache(A, BUFFER_SIZE);
    
    
    Test(8, 8, 16, 16, 16 * 64);

    // Main loop
    while (aptMainLoop())
    {
        hidScanInput();

        // Respond to user input
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START)
            break; // break in order to return to hbmenu
            
        if (kDown & KEY_A) {
            auto command = GetCommand();
            unsigned widthA, gapA, widthB, gapB, length;
            sscanf(command.c_str(), "%u %u %u %u %u", &widthA, &gapA, &widthB, &gapB, &length);
            Test(widthA, gapA, widthB, gapB, length);
        }
        
        gfxFlushBuffers();
        gspWaitForVBlank();
        gfxSwapBuffers();


    }

	gfxExit();
    return 0;
}
