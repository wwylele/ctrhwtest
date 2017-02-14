#include <string>
#include <vector>
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct TestCase {
    std::vector<u8> plain;
    std::vector<u8> cipher;
    u32 nonce_size, nonce_offset;
    void PrintInfo() {
        printf("size=%zu, nonce_size=%lu, nonce_offset=%lu", plain.size(), nonce_size,
               nonce_offset);
    }
};

std::vector<TestCase> test_cases;
bool check_cipher = false;

/*
void GenerateTestCases() {
    printf("Generate Test Cases...");
    test_cases.clear();
    for (u32 size : {2, 3, 4, 5, 8, 12, 16, 32, 33, 34, 35, 36, 100, 999}) {
        for (u32 nonce_offset : {0, 1, 2, 3, 4, 8, 10, 20, 40}) {
            for (u32 nonce_size : {1, 2, 3, 4, 5, 8, 11, 12, 13, 14}) {

                if (nonce_offset + nonce_size > size)
                    continue;
                if (nonce_offset == 0 && nonce_size == size)
                    continue;

                test_cases.push_back(
                    TestCase{std::vector<u8>(size), std::vector<u8>{}, nonce_size, nonce_offset});
                for (u8& byte : test_cases.back().plain) {
                    byte = static_cast<u8>(rand() & 0xFF);
                }
            }
        }
    }
    check_cipher = false;
    printf("Finished\n");
}

void SaveTestCases() {
    printf("Save Test Cases...");

    FILE* file = fopen("wrap_test_case.bin", "wb");
    if (!file) {
        printf("Failed!\n");
        return;
    }
    u32 count = test_cases.size();
    fwrite(&count, 4, 1, file);

    for (auto& test : test_cases) {
        u32 size = test.plain.size();
        fwrite(&size, 4, 1, file);
        fwrite(&test.nonce_size, 4, 1, file);
        fwrite(&test.nonce_offset, 4, 1, file);
        fwrite(test.plain.data(), size, 1, file);
        fwrite(test.cipher.data(), size + 16, 1, file);
    }

    fclose(file);

    printf("Finished\n");
}
*/
void LoadTestCases() {
    printf("Load Test Cases...");

    FILE* file = fopen("wrap_test_case.bin", "rb");
    if (!file) {
        printf("Failed!\n");
        return;
    }

    u32 count;
    fread(&count, 4, 1, file);
    test_cases.resize(count);

    for (auto& test : test_cases) {
        u32 size;
        fread(&size, 4, 1, file);
        test.plain.resize(size);
        test.cipher.resize(size + 16);
        fread(&test.nonce_size, 4, 1, file);
        fread(&test.nonce_offset, 4, 1, file);
        fread(test.plain.data(), size, 1, file);
        fread(test.cipher.data(), size + 16, 1, file);
    }

    fclose(file);

    check_cipher = true;

    printf("Finished\n");
}

Handle aptHandle;

static inline int countPrmWords(u32 hdr) {
    return (hdr & 0x3F) + ((hdr >> 6) & 0x3F);
}

std::vector<u32> ExeRequestSilent(const std::vector<u32>& request) {
    u32* cmdbuf = getThreadCommandBuffer();
    memcpy(cmdbuf, request.data(), request.size() * 4);
    Result r = svcSendSyncRequest(aptHandle);
    if (R_FAILED(r)) {
        return {0, *(u32*)&r};
    }
    return std::vector<u32>(cmdbuf, cmdbuf + 1 + countPrmWords(cmdbuf[0]));
}

bool ParseResult(Result r) {
    if (r == 0) {
        printf("\x1b[32mSuccess\x1b[0m\n");
        return true;
    } else {
        printf("\x1b[31mError: %08lX\x1b[0m\n", r);
        return false;
    }
}

static void PrintData(const std::vector<u8>& buffer) {
    for (unsigned i = 0; i < buffer.size(); ++i) {
        if (i % 16 == 0)
            printf("\n");
        printf("%02X ", buffer[i]);
    }
    printf("\n");
}

void RunTestCases() {
    printf("Run %zu test cases (cipher check %s)\n", test_cases.size(),
           check_cipher ? "on" : "off");
    int count = 0, successed = 0;
    for (auto& test : test_cases) {
        bool failed = false;
        std::vector<u8> cipher(test.plain.size() + 16);
        u32 result = ExeRequestSilent({0x00460104, cipher.size(), test.plain.size(),
                                       test.nonce_offset, test.nonce_size,
                                       (test.plain.size() << 4) | 0xA, (u32)test.plain.data(),
                                       (cipher.size() << 4) | 0xC, (u32)cipher.data()})[1];
        if (result != 0) {
            test.PrintInfo();
            printf(" Wrap ");
            ParseResult(result);
        }

        if (check_cipher) {
            if (test.cipher != cipher) {
                test.PrintInfo();
                printf(" cipher mismatch! (size=%zu, nonce_offset=%lu, nonce_size=%lu\n",
                       test.plain.size(), test.nonce_offset, test.nonce_size);
                /*
                printf("expected:");
                PrintData(test.cipher);
                printf("actual:");
                PrintData(cipher);
                */
                failed = true;
            }
        } else {
            test.cipher = cipher;
        }

        std::vector<u8> rpdata(test.plain.size());
        result = ExeRequestSilent({0x00470104, rpdata.size(), cipher.size(), test.nonce_offset,
                                   test.nonce_size, (cipher.size() << 4) | 0xA, (u32)cipher.data(),
                                   (rpdata.size() << 4) | 0xC, (u32)rpdata.data()})[1];

        if (result != 0) {
            test.PrintInfo();
            printf(" Unwrap ");
            ParseResult(result);
        }

        if (rpdata != test.plain) {
            test.PrintInfo();
            printf(" plain data mismatch! (size=%zu, nonce_offset=%lu, nonce_size=%lu)\n",
                   test.plain.size(), test.nonce_offset, test.nonce_size);
            /*
            printf("expected:");
            PrintData(test.plain);
            printf("actual:");
            PrintData(rpdata);
            */
            failed = true;
        }

        if (!failed)
            ++successed;
        ++count;
    }
    check_cipher = true;
    printf("Finished! [%d/%d] passed\n", successed, count);
}

int main() {
    aptInit();
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    printf("Get APT:U\n");
    ParseResult(srvGetServiceHandle(&aptHandle, "APT:U"));

    u32 kDown;
    while (aptMainLoop()) {
        hidScanInput();
        kDown = hidKeysDown();

        if (kDown & KEY_START) {
            break;
        } /*else if (kDown & KEY_A) {
            RunTestCases();
        } else if (kDown & KEY_B) {
            GenerateTestCases();
        } else if (kDown & KEY_X) {
            SaveTestCases();
        } else if (kDown & KEY_Y) {
            LoadTestCases();
        }*/
        if (kDown & KEY_A) {
            LoadTestCases();
            RunTestCases();
        }

        gfxFlushBuffers();
        gspWaitForVBlank();
        gfxSwapBuffers();
    }

    svcCloseHandle(aptHandle);

    gfxExit();
    return 0;
}
