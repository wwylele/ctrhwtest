#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <3ds.h>
#include <vector>
#include <string>
#include <map>

enum ParamType {
    Value,
    Width,
    Height,
    Port,
    Camera,
    Context,
    XA,
    YA,
    XB,
    YB,
};

struct CommandStruct{
    u32 header;
    std::vector<ParamType> params;
};

bool ParseResult(Result r) {
    if (r == 0) {
        printf("\x1b[32mSuccess\x1b[0m\n");
        return true;
    } else {
        printf("\x1b[31mError: %08lX\x1b[0m\n", r);
        return false;
    }
}

const std::map<std::string, CommandStruct> easy_commands {
    {"start", {0x00010040, {Port}}},
    {"stop", {0x00020040, {Port}}},
    {"busy", {0x00030040, {Port}}},
    {"clear", {0x00040040, {Port}}},
    // 00050040
    // 00060040
    // 00070102
    {"finished", {0x00080040, {Port}}},
    {"line", {0x00090100, {Port, Value, Width, Height}}},
    {"maxline", {0x000A0080, {Width, Height}}},
    {"byte", {0x000B0100, {Port, Value, Width, Height}}},
    {"gbyte", {0x000C0040, {Port}}},
    {"maxbyte", {0x000D0080, {Width, Height}}},
    {"trim", {0x000E0080, {Port, Value}}},
    {"itrim", {0x000F0040, {Port}}},
    {"strim", {0x00100140, {Port, XA, YA, XB, YB}}},
    {"gtrim", {0x00110040, {Port}}},
    {"ctrim", {0x00120140, {Port, XA, YA, XB, YB}}},
    {"act", {0x00130040, {Camera}}},
    {"ctxt", {0x00140080, {Camera, Context}}},
    {"expo", {0x00150080, {Camera, Value}}},
    {"white", {0x00160080, {Camera, Value}}},
    {"whiteb", {0x00170080, {Camera, Value}}},
    {"sharp", {0x00180080, {Camera, Value}}},
    {"autoe", {0x00190080, {Camera, Value}}},
    {"iautoe", {0x001A0040, {Camera}}},
    {"autow", {0x001B0080, {Camera, Value}}},
    {"iautow", {0x001C0040, {Camera}}},
    {"flip", {0x001D00C0, {Camera, Value, Context}}},
    {"dsize", {0x001E0200, {Camera, Width, Height, XA, YA, XB, YB, Context}}},
    {"size", {0x001F00C0, {Camera, Value, Context}}},
    {"rate", {0x00200080, {Camera, Value}}},
    {"mode", {0x00210080, {Camera, Value}}},
    {"effect", {0x002200C0, {Camera, Value, Context}}},
    {"contrast", {0x00230080, {Camera, Value}}},
    {"lens", {0x00240080, {Camera, Value}}},
    {"fmt", {0x002500C0, {Camera, Value, Context}}},
    {"autoew", {0x00260140, {Camera, XA, YA, Width, Height}}},
    {"autoww", {0x00270140, {Camera, XA, YA, Width, Height}}},
    {"filter", {0x00280080, {Camera, Value}}},

    {"init", {0x00390000, {}}},
    {"fina", {0x003A0000, {}}},
};

Handle camHandle;
std::vector<u16> bufA, bufB(400 * 240);
u32 bufW, bufH, bufBytes;

void VerifyEasyCommandTable() {
    for (auto& pair : easy_commands) {
        u32 command = pair.second.header;
        if (((command >> 6) & 0x3F) != pair.second.params.size())
            printf("ill command %s\n", pair.first.c_str());
    }
}

std::vector<std::string> SplitString(std::string str, std::string pattern) {
    std::string::size_type pos;
    std::vector<std::string> result;
    str += pattern;
    std::string::size_type size = str.size();

    for(std::string::size_type i = 0; i < size; i++) {
        pos = str.find(pattern, i);
        if(pos < size) {
            std::string s = str.substr(i, pos - i);
            result.push_back(s);
            i = pos + pattern.size() - 1;
        }
    }
    return result;
}

u32 IdGroup(const std::string& str) {
    u32 result = 0;
    for (char c : str) {
        if (c > '9' || c < '0') {
            break;
        }
        result |= 1 << (c - '0');
    }
    return result;
}

static inline int countPrmWords(u32 hdr)
{
    return (hdr&0x3F) + ((hdr>>6)&0x3F);
}

std::vector<u32> ExeRequestSilent(const std::vector<u32>& request) {
    u32* cmdbuf = getThreadCommandBuffer();
    memcpy(cmdbuf, request.data(), request.size() * 4);
    Result r = svcSendSyncRequest(camHandle);
    if (R_FAILED(r)) {
        return {0, *(u32*)&r};
    }
    return std::vector<u32>(cmdbuf, cmdbuf + 1 + countPrmWords(cmdbuf[0]));
}

void ExeRequest(const std::vector<u32>& request) {
    printf("<");
    for (u32 i : request) {
        printf("%08lX ", i);
    }
    printf(">\n");

    std::vector<u32> response = ExeRequestSilent(request);
    printf(response[1] == 0 ? "\x1b[32m<" : "\x1b[31m<");
    for (u32 i : response) {
        printf("%08lX ", i);
    }
    printf(">\x1b[0m\n");

}

int GetMaxBytes_Guess(int width, int height) {
    if (width * height * 2 % 256 != 0) {
        return -1;
    }
    u32 bytes = 5120;
    while (width * height * 2 % bytes != 0) {
        bytes -= 256;
    }
    return bytes;
}

int GetMaxBytes_Actual(int width, int height) {
    u32* cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = 0x000D0080;
    cmdbuf[1] = width;
    cmdbuf[2] = height;
    svcSendSyncRequest(camHandle);
    if (cmdbuf[1] != 0) return -1;
    return cmdbuf[2];
}

int GetMaxLines_Guess(int width, int height) {
    if (width * height * 2 % 256 != 0) {
        return -1;
    }

    int lines = 2560 / width;
    if (lines > height) {
        lines = height;
    }
    while (height % lines != 0 || (lines * width * 2 % 256 != 0)) {
        --lines;
        if (lines == 0) {
            return -1;
        }
    }
    return lines;
}

int GetMaxLines_Actual(int width, int height) {
    u32* cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = 0x000A0080;
    cmdbuf[1] = width;
    cmdbuf[2] = height;
    svcSendSyncRequest(camHandle);
    if (cmdbuf[1] != 0) return -1;
    return cmdbuf[2];
}

void ProcessCommand() {
    static SwkbdState swkbd;
    static char mybuf[60];

    swkbdInit(&swkbd, SWKBD_TYPE_WESTERN, 2, -1);
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    swkbdSetHintText(&swkbd, "Please input a command.");

    if (SWKBD_BUTTON_RIGHT != swkbdInputText(&swkbd, mybuf, sizeof(mybuf)))
        return;

    std::string command(mybuf);
    auto params = SplitString(command, " ");
    if (params.size() == 0) {
        printf("Empty command!\n");
        return;
    }

    auto easy = easy_commands.find(params[0]);
    if (easy != easy_commands.end()) {
        u32 aValue = 0;
        u32 aWidth = 0;
        u32 aHeight = 0;
        u32 aPort = 0;
        u32 aCamera = 0;
        u32 aContext = 0;
        u32 aXA = 0;
        u32 aYA = 0;
        u32 aXB = 0;
        u32 aYB = 0;
        for (unsigned i = 1; i < params.size(); ++i) {
            if (params[i].size() < 1) continue;
            auto content = params[i].substr(1);
            switch (params[i][0]) {
            case 'v': aValue = strtoul(content.data(), 0, 0); break;
            case 'w': aWidth = strtoul(content.data(), 0, 0); break;
            case 'h': aHeight = strtoul(content.data(), 0, 0); break;
            case 'p': aPort = IdGroup(content); break;
            case 'c': aCamera = IdGroup(content); break;
            case 't': aContext = IdGroup(content); break;
            case 'x': aXA = strtoul(content.data(), 0, 0); break;
            case 'y': aYA = strtoul(content.data(), 0, 0); break;
            case 'm': aXB = strtoul(content.data(), 0, 0); break;
            case 'n': aYB = strtoul(content.data(), 0, 0); break;
            default :
                printf("Unknown param %c\n", params[i][0]);
                return;
            }
        }
        auto& command = easy->second;
        std::vector<u32> request;
        request.push_back(command.header);
        for (auto type : command.params) {
            switch (type) {
            case Value: request.push_back(aValue); break;
            case Width: request.push_back(aWidth); break;
            case Height: request.push_back(aHeight); break;
            case Port: request.push_back(aPort); break;
            case Camera: request.push_back(aCamera); break;
            case Context: request.push_back(aContext); break;
            case XA: request.push_back(aXA); break;
            case YA: request.push_back(aYA); break;
            case XB: request.push_back(aXB); break;
            case YB: request.push_back(aYB); break;
            }
        }
        ExeRequest(request);
    } else if (params[0] == "buf") {
        for (unsigned i = 1; i < params.size(); ++i) {
            if (params[i].size() < 1) continue;
            auto content = params[i].substr(1);
            switch (params[i][0]) {
            case 'w': bufW = strtoul(content.data(), 0, 0); break;
            case 'h': bufH = strtoul(content.data(), 0, 0); break;
            case 'b': bufBytes = strtoul(content.data(), 0, 0); break;
            default :
                printf("Unknown param %c\n", params[i][0]);
                return;
            }
        }
        bufA.resize(bufW * bufH);
        printf("buffer width = %lu, height = %lu, bytes = %lu\n", bufW, bufH, bufBytes);
    } else if (params[0] == "testb") {
        for (int height = 1; height < 641; ++height) {
            for (int width = 1; width < 481; ++width) {
                int guess = GetMaxBytes_Guess(width, height);
                int actual = GetMaxBytes_Actual(width, height);
                if (actual != guess) {
                    printf("mismatch at w=%d, h=%d, guess=%d, actual=%d\n", width, height, guess, actual);
                }
            }
            printf("tested h=%d\n", height);
        }
        printf("testb finished\n");
    } else if (params[0] == "testl") {
        for (int height = 1; height < 641; ++height) {
            for (int width = 1; width < 481; ++width) {
                int guess = GetMaxLines_Guess(width, height);
                int actual = GetMaxLines_Actual(width, height);
                if (actual != guess) {
                    printf("mismatch at w=%d, h=%d, guess=%d, actual=%d\n", width, height, guess, actual);
                }
            }
            printf("tested h=%d\n", height);
        }
        printf("testl finished\n");
    } else {
        printf("Unknown command %s\n", params[0].c_str());
    }

}

void writePictureToFramebufferRGB565(void *fb, void *img, u16 x, u16 y, u16 width, u16 height) {
    u8 *fb_8 = (u8*) fb;
    u16 *img_16 = (u16*) img;
    int i, j, draw_x, draw_y;
    for(j = 0; j < height; j++) {
        for(i = 0; i < width; i++) {
            draw_y = y + height - j;
            draw_x = x + i;
            u32 v = (draw_y + draw_x * height) * 3;
            u16 data = img_16[j * width + i];
            uint8_t b = ((data >> 11) & 0x1F) << 3;
            uint8_t g = ((data >> 5) & 0x3F) << 2;
            uint8_t r = (data & 0x1F) << 3;
            fb_8[v] = r;
            fb_8[v+1] = g;
            fb_8[v+2] = b;
        }
    }
}

#define WAIT_TIMEOUT 1000000000ULL

int main() {
    aptInit();
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);

    VerifyEasyCommandTable();

    printf("Get cam:u\n");
    ParseResult(srvGetServiceHandle(&camHandle, "cam:u"));

    u32 kDown, kHeld;
    int port_receiving = -1;
    Handle event;
    while (aptMainLoop()) {
        std::vector<u32> response;
        hidScanInput();
        kDown = hidKeysDown();
        kHeld = hidKeysHeld();

        if ((kDown & KEY_A) && port_receiving == -1) {
            ProcessCommand();
        }

        if (kHeld & KEY_L) {
            if (port_receiving == -1) {
                port_receiving = 0;
                response = ExeRequestSilent(std::vector<u32>{0x00010040, 1});
                if (response[1] != 0) {
                    printf("StartCapture:");
                    ParseResult(*(Result*)&response[1]);
                }
            }
        } else {
            if (port_receiving == 0) {
                port_receiving = -1;
                response = ExeRequestSilent(std::vector<u32>{0x00020040, 1});
                if (response[1] != 0) {
                    printf("StartCapture:");
                    ParseResult(*(Result*)&response[1]);
                }
            }
        }

        if (kHeld & KEY_R) {
            if (port_receiving == -1) {
                port_receiving = 1;
                response = ExeRequestSilent(std::vector<u32>{0x00010040, 2});
                if (response[1] != 0) {
                    printf("StartCapture:");
                    ParseResult(*(Result*)&response[1]);
                }
            }
        } else {
            if (port_receiving == 1) {
                port_receiving = -1;
                response = ExeRequestSilent(std::vector<u32>{0x00020040, 2});
                if (response[1] != 0) {
                    printf("StartCapture:");
                    ParseResult(*(Result*)&response[1]);
                }
            }
        }

        if (kDown & KEY_START) {
            break;
        }

        if (port_receiving != -1) {
            memset(bufA.data(), 0xCC, bufA.size() * 2);
            memset(bufB.data(), 0, bufB.size() * 2);

            response = ExeRequestSilent(std::vector<u32>{0x00070102,
                (u32)bufA.data(), (u32)(1 << port_receiving), bufA.size() * 2, bufBytes,
                IPC_Desc_SharedHandles(1), CUR_PROCESS_HANDLE});
            if (response[1] != 0) {
                printf("SetReceiving:");
                ParseResult(*(Result*)&response[1]);
            }
            event = response[3];

            Result r;
            if (0 != (r = svcWaitSynchronization(event, WAIT_TIMEOUT))) {
                printf("svcWaitSynchronization:");
                ParseResult(r);
            }

            for (u32 x = 0; x < std::min(400UL, bufW); ++x) {
                for (u32 y = 0; y < std::min(240UL, bufH); ++y) {
                    bufB[x + y * 400] = bufA[x + y * bufW];
                }
            }

            printf("tick frame\n");
            writePictureToFramebufferRGB565(gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL), bufB.data(), 0, 0, 400, 240);

            svcCloseHandle(event);
        }

        gfxFlushBuffers();
        gspWaitForVBlank();
        gfxSwapBuffers();

    }

    svcCloseHandle(camHandle);


    gfxExit();
    aptExit();
    return 0;
}
