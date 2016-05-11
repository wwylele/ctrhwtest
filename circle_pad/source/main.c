#include <3ds.h>
#include <stdio.h>
#include <math.h>

int main(int argc, char **argv)
{
	gfxInitDefault();

	consoleInit(GFX_TOP, NULL);

	float TAN30 = 0.577350269;
        float TAN60 = 1/TAN30;

	while (aptMainLoop())
	{
		hidScanInput();

		u32 kDown = hidKeysDown();
		u32 kHeld = hidKeysHeld();

		if (kDown & KEY_START) break;
	
		printf("\x1b[1;0H%c%c%c%c <- read from shared memory", 
			(kHeld & KEY_CPAD_RIGHT)?'R':' ',
			(kHeld & KEY_CPAD_LEFT)? 'L':' ',
			(kHeld & KEY_CPAD_UP)?   'U':' ',
			(kHeld & KEY_CPAD_DOWN)? 'D':' ');

		circlePosition pos;

		hidCircleRead(&pos);

		printf("\x1b[3;0H%04d; %04d", pos.dx, pos.dy);

		int r,l,u,d;
		r = l = u = d = 0;
		if(pos.dx*pos.dx + pos.dy*pos.dy > 1600){
			float tan = fabs((float)pos.dy/pos.dx);	
			if (pos.dx != 0 && tan < TAN60) {
				if (pos.dx > 0)
					r=1;
				else
					l=1;
			}
			if (pos.dx == 0 || tan > TAN30) {
				if (pos.dy > 0)
					u=1;
				else
					d=1;;
			}
		}
		
		printf("\x1b[2;0H%c%c%c%c <- estimated by this test", 
			r?'R':' ',
			l?'L':' ',
			u?'U':' ',
			d?'D':' ');

        	gfxFlushBuffers();
        	gfxSwapBuffers();
	
        	gspWaitForVBlank();
	}

	gfxExit();
	return 0;
}
