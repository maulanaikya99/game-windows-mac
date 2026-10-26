#include <windows.h>
#include <stdio.h>
typedef struct { DWORD pkt; WORD b; BYTE lt, rt; SHORT lx, ly, rx, ry; } ST;
int main(int argc, char **argv) {
    HMODULE m = LoadLibraryA(argc > 1 ? argv[1] : "xinput9_1_0.dll");
    DWORD (WINAPI *get)(DWORD, ST *) = (void *)GetProcAddress(m, "XInputGetState");
    if (!get) { printf("load failed\n"); return 1; }
    int secs = argc > 2 ? atoi(argv[2]) : 1;
    DWORD last[4] = {0};
    for (int t = 0; t < secs * 20; t++) {
        for (int i = 0; i < 4; i++) {
            ST s; DWORD r = get(i, &s);
            if (t == 0) printf("pad %d: %s\n", i, r ? "not present" : "CONNECTED");
            if (!r && s.pkt != last[i]) { last[i] = s.pkt; printf("pad%d btn=%04x LT=%d RT=%d L=(%d,%d) R=(%d,%d)\n", i, s.b, s.lt, s.rt, s.lx, s.ly, s.rx, s.ry); fflush(stdout); }
        }
        Sleep(50);
    }
    return 0;
}
