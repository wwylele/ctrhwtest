

#include <3ds.h>
#include <stdio.h>
#include <string.h>
extern Handle hidHandle;
extern vu32* hidSharedMem;
extern Handle hidEvents[5];

// instead using angularRate from ctrulib, I use this.
typedef struct{
    s16 x;
    s16 y; // don't swap y and z
    s16 z;
} myAngularRate;

bool hidTryWaitForEvent(HID_Event id, s64 nanoseconds){
    if(R_DESCRIPTION(svcWaitSynchronization(hidEvents[id], nanoseconds))==RD_TIMEOUT){
        return false;
    } else{
        svcClearEvent(hidEvents[id]);
        return true;
    }
        
}

Result HIDUSER_GetGyroscopeLowCalibrateParam(u8 result[20]){
    u32* cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(0x16, 0, 0); // 0x160000

    Result ret = 0;
    if(R_FAILED(ret = svcSendSyncRequest(hidHandle)))return ret;

    memcpy(result, cmdbuf+2, 20);

    return cmdbuf[1];
}

// ctrulib bug (PR#269)
Result HIDUSER_GetGyroscopeRawToDpsCoefficient_(float *coeff){
    u32* cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(0x15, 0, 0); // 0x150000

    Result ret = 0;
    if(R_FAILED(ret = svcSendSyncRequest(hidHandle)))return ret;

    *coeff = *(float*)(cmdbuf+2);

    return cmdbuf[1];
}

int main(int argc, char **argv){

    // Initialize services
    gfxInitDefault();

    //Initialize console on top screen. Using NULL as the second argument tells the console library to use the internal console structure as current one
    consoleInit(GFX_TOP, NULL);


    int alevel=0, glevel = 0;

    //print GyroscopeRawToDpsCoefficient
    float gyroCoef;
    HIDUSER_GetGyroscopeRawToDpsCoefficient_(&gyroCoef);
    printf("\x1b[0;0HGyroCoef:%f", gyroCoef);

    //print GyroscopeLowCalibrateParam
    u8 Calibrate[20];
    HIDUSER_GetGyroscopeLowCalibrateParam(Calibrate);
    for(int i = 0; i<3; ++i){
        printf("\x1b[%d;0HGyroCalibrate(%d):", i+1, i+1);
        for(int j = 0; j<6; ++j){
            printf("%02X ", Calibrate[i*6+j]);
        }
    }


    printf("\x1b[13;0HPad:\n"); 
    printf("A:call EnableGyroscope\n");
    printf("B:call DisableGyroscope\n");
    printf("X:call EnableAccelerometer\n");
    printf("Y:call DisableAccelerometer\n");

    Result result=0;
    while(aptMainLoop()){
        hidScanInput();

        u32 kDown = hidKeysDown();

        if(kDown & KEY_START) break; // break in order to return to hbmenu
        if(kDown & KEY_A){
            ++glevel;
            result=HIDUSER_EnableGyroscope();
        }
        if(kDown & KEY_B){
            --glevel;
            result = HIDUSER_DisableGyroscope();
        }
        if(kDown & KEY_X){
            ++alevel;
            result = HIDUSER_EnableAccelerometer();
        }
        if(kDown & KEY_Y){
            --alevel;
            result = HIDUSER_DisableAccelerometer();
        }


        //Read gyro data
        myAngularRate gyro;
        hidGyroRead((angularRate*)&gyro);
        printf("\x1b[5;0Hgyro(level=%3d)%6d;%6d;%6d",glevel, gyro.x, gyro.y, gyro.z);

        //Read raw gyro data
        gyro = *(myAngularRate*)&hidSharedMem[86+6];
        printf("\x1b[6;0Hgyro(raw      )%6d;%6d;%6d", gyro.x, gyro.y, gyro.z);

        //Read accel data
        accelVector vector;
        hidAccelRead(&vector);
        printf("\x1b[7;0Hacce(level=%3d)%6d;%6d;%6d",alevel, vector.x, vector.y, vector.z);

        //Read raw accel
        vector = *(accelVector*)&hidSharedMem[66+6];
        printf("\x1b[8;0Hacce(raw      )%6d;%6d;%6d", vector.x, vector.y, vector.z);

        //test if gyro and accel events are activated
        printf("\x1b[9;0Hgyro event: %s", hidTryWaitForEvent(HIDEVENT_Gyro, 10000000) ? "true " : "false");
        printf("\x1b[10;0Hacce event: %s", hidTryWaitForEvent(HIDEVENT_Accel, 10000000)?"true ":"false");

        //print the last result of enable/disable gyroscope/accelerometer call
        printf("\x1b[11;0Henable/disable call result=%08lX", result);

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
