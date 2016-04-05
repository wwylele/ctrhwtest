
#include <3ds.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{

	// Initialize services
	gfxInitDefault();

	//Initialize console on top screen. Using NULL as the second argument tells the console library to use the internal console structure as current one
	consoleInit(GFX_TOP, NULL);

    

	// Main loop
	while (aptMainLoop())
	{
		//Scan all the inputs. This should be done once for each frame
		hidScanInput();

		//hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
		u32 kDown = hidKeysDown();

		if (kDown & KEY_START) break; // break in order to return to hbmenu

        printf("\x1b[0;0H shared page:\n");

        volatile u8 *spbase;
        spbase = (volatile u8 *)0x1FF81000;

        for(int i = 0; i<16; ++i){
            for(int j = 0; j<16; ++j){
                printf("%02X ", *spbase);
                ++spbase;
            }
            printf("\n");
        }

		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();

		//Wait for VBlank
		gspWaitForVBlank();
	}



	// Exit services
	gfxExit();
	return 0;
}
