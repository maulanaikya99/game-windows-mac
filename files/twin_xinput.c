/* Replacement xinput9_1_0.dll / xinput1_3.dll for the "Twin USB Joystick" adapter (2563:0555).
 *
 * Wine splits this adapter into two HID devices (COL01 = port 1, COL02 = port 2), but does
 * not mark them as XInput gamepads, and the game's DirectInput path only reads some of the
 * buttons. This DLL reads both HID devices directly and presents them as two XInput
 * controllers with the correct PlayStation -> Xbox layout.
 *
 * Pad HID order (same as the SDL mapping in GoW3): button 1 Triangle, 2 Circle, 3 Cross,
 * 4 Square, 5 L1, 6 R1, 7 L2, 8 R2, 9 Select, 10 Start, 11 L3, 12 R3; X/Y axes left stick,
 * Z/Rz right stick, hat = D-pad. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <stdio.h>
#include <wchar.h>

/* TWIN_XINPUT_LOG=<file> -> diagnostic log to that file (GUI games have no stderr). */
static FILE *log_file;
#define LOG(...) do { if (log_file) { fprintf(log_file, "twin-xinput: " __VA_ARGS__); fflush(log_file); } } while (0)

#define XUSER_MAX_COUNT 4
#define XINPUT_GAMEPAD_DPAD_UP 0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN 0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT 0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT 0x0008
#define XINPUT_GAMEPAD_START 0x0010
#define XINPUT_GAMEPAD_BACK 0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB 0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB 0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER 0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER 0x0200
#define XINPUT_GAMEPAD_A 0x1000
#define XINPUT_GAMEPAD_B 0x2000
#define XINPUT_GAMEPAD_X 0x4000
#define XINPUT_GAMEPAD_Y 0x8000

typedef struct { WORD wButtons; BYTE bLeftTrigger, bRightTrigger; SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY; } XINPUT_GAMEPAD;
typedef struct { DWORD dwPacketNumber; XINPUT_GAMEPAD Gamepad; } XINPUT_STATE;
typedef struct { WORD wLeftMotorSpeed, wRightMotorSpeed; } XINPUT_VIBRATION;
typedef struct { BYTE Type, SubType; WORD Flags; XINPUT_GAMEPAD Gamepad; XINPUT_VIBRATION Vibration; } XINPUT_CAPABILITIES;
typedef struct { BYTE BatteryType, BatteryLevel; } XINPUT_BATTERY_INFORMATION;
typedef struct { WORD VirtualKey; WCHAR Unicode; WORD Flags; BYTE UserIndex, HidCode; } XINPUT_KEYSTROKE;

/* HID buttons 1..12 -> XInput bits (0 = unused; L2/R2 become triggers). */
static const WORD button_map[13] = {
    0, XINPUT_GAMEPAD_Y, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_X,
    XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, 0, 0,
    XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_START, XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
};

struct pad {
    HANDLE handle;
    PHIDP_PREPARSED_DATA preparsed;
    HIDP_CAPS caps;
    LONG min[4], max[4];   /* X, Y, Z, Rz */
    LONG hat_min, hat_max;
    BOOL has_hat;
    CRITICAL_SECTION lock;
    XINPUT_STATE state;
    BOOL connected;
};

static struct pad pads[XUSER_MAX_COUNT];
static INIT_ONCE init_once = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION scan_lock;
static DWORD last_scan;
static BOOL enabled = TRUE;
static const USAGE axis_usage[4] = {0x30, 0x31, 0x32, 0x35};

static SHORT scale_axis(LONG v, LONG lo, LONG hi, BOOL invert)
{
    if (hi <= lo) return 0;
    double f = ((double)(v - lo) / (hi - lo)) * 2.0 - 1.0;   /* -1..1 */
    if (invert) f = -f;
    if (f > -0.08 && f < 0.08) f = 0;                       /* center dead zone */
    LONG r = (LONG)(f * 32767.0);
    if (r < -32768) r = -32768;
    if (r > 32767) r = 32767;
    return (SHORT)r;
}

static void parse_report(struct pad *p, char *report, ULONG len)
{
    XINPUT_GAMEPAD g = {0};
    USAGE usages[32];
    ULONG n = ARRAYSIZE(usages), v;

    if (HidP_GetUsages(HidP_Input, HID_USAGE_PAGE_BUTTON, 0, usages, &n, p->preparsed, report, len) == HIDP_STATUS_SUCCESS)
    {
        for (ULONG i = 0; i < n; i++)
        {
            USAGE u = usages[i];
            if (u == 7) g.bLeftTrigger = 255;
            else if (u == 8) g.bRightTrigger = 255;
            else if (u < ARRAYSIZE(button_map)) g.wButtons |= button_map[u];
        }
    }
    SHORT axes[4] = {0};
    for (int a = 0; a < 4; a++)
        if (HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC, 0, axis_usage[a], &v, p->preparsed, report, len) == HIDP_STATUS_SUCCESS)
            axes[a] = scale_axis((LONG)v, p->min[a], p->max[a], a == 1 || a == 3);
    g.sThumbLX = axes[0]; g.sThumbLY = axes[1]; g.sThumbRX = axes[2]; g.sThumbRY = axes[3];

    if (p->has_hat && HidP_GetUsageValue(HidP_Input, HID_USAGE_PAGE_GENERIC, 0, 0x39, &v, p->preparsed, report, len) == HIDP_STATUS_SUCCESS)
    {
        static const WORD hat[8] = {
            XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT,
            XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_DOWN,
            XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_LEFT,
            XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_UP,
        };
        LONG h = (LONG)v - p->hat_min;
        if (h >= 0 && h < 8 && (LONG)v <= p->hat_max) g.wButtons |= hat[h];
    }

    EnterCriticalSection(&p->lock);
    if (memcmp(&g, &p->state.Gamepad, sizeof(g)))
    {
        p->state.Gamepad = g;
        p->state.dwPacketNumber++;
    }
    LeaveCriticalSection(&p->lock);
}

static DWORD WINAPI reader_thread(void *arg)
{
    struct pad *p = arg;
    ULONG len = p->caps.InputReportByteLength;
    char *buf = HeapAlloc(GetProcessHeap(), 0, len);
    OVERLAPPED ov = {0};
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    for (;;)
    {
        DWORD got = 0;
        ResetEvent(ov.hEvent);
        if (!ReadFile(p->handle, buf, len, NULL, &ov) && GetLastError() != ERROR_IO_PENDING) break;
        if (!GetOverlappedResult(p->handle, &ov, &got, TRUE)) break;
        if (got) parse_report(p, buf, got);
    }
    EnterCriticalSection(&p->lock);
    p->connected = FALSE;
    memset(&p->state.Gamepad, 0, sizeof(p->state.Gamepad));
    LeaveCriticalSection(&p->lock);
    return 0;
}

static void setup_axes(struct pad *p)
{
    USHORT n = p->caps.NumberInputValueCaps;
    HIDP_VALUE_CAPS *vc = HeapAlloc(GetProcessHeap(), 0, sizeof(*vc) * (n ? n : 1));
    for (int a = 0; a < 4; a++) { p->min[a] = 0; p->max[a] = 255; }
    if (HidP_GetValueCaps(HidP_Input, vc, &n, p->preparsed) == HIDP_STATUS_SUCCESS)
    {
        for (USHORT i = 0; i < n; i++)
        {
            if (vc[i].UsagePage != HID_USAGE_PAGE_GENERIC) continue;
            USAGE lo = vc[i].IsRange ? vc[i].Range.UsageMin : vc[i].NotRange.Usage;
            USAGE hi = vc[i].IsRange ? vc[i].Range.UsageMax : vc[i].NotRange.Usage;
            for (USAGE u = lo; u <= hi; u++)
            {
                for (int a = 0; a < 4; a++)
                    if (u == axis_usage[a]) { p->min[a] = vc[i].LogicalMin; p->max[a] = vc[i].LogicalMax; }
                if (u == 0x39) { p->has_hat = TRUE; p->hat_min = vc[i].LogicalMin; p->hat_max = vc[i].LogicalMax; }
            }
        }
    }
    HeapFree(GetProcessHeap(), 0, vc);
}

static BOOL path_is_twin(const WCHAR *path, int *port)
{
    WCHAR low[512];
    int i;
    for (i = 0; path[i] && i < 511; i++) low[i] = towlower(path[i]);
    low[i] = 0;
    if (!wcsstr(low, L"vid_2563&pid_0555")) return FALSE;
    if (wcsstr(low, L"col01")) *port = 0;
    else if (wcsstr(low, L"col02")) *port = 1;
    else return FALSE;
    return TRUE;
}

static BOOL CALLBACK do_init(INIT_ONCE *once, void *param, void **ctx)
{
    for (int i = 0; i < XUSER_MAX_COUNT; i++) InitializeCriticalSection(&pads[i].lock);
    InitializeCriticalSection(&scan_lock);
    char log_path[MAX_PATH];
    if (GetEnvironmentVariableA("TWIN_XINPUT_LOG", log_path, sizeof(log_path)) - 1 < sizeof(log_path) - 1)
        log_file = fopen(log_path, "a");
    return TRUE;
}

/* Wine adds HID devices a while after wineserver starts, and pads can be unplugged and
 * replugged, so rescan (at most every 2 seconds) while a port is still empty. */
static void scan_devices(void)
{
    GUID hid_guid;
    HidD_GetHidGuid(&hid_guid);
    HDEVINFO set = SetupDiGetClassDevsW(&hid_guid, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    SP_DEVICE_INTERFACE_DATA iface = {sizeof(iface)};
    char buffer[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) + 1024 * sizeof(WCHAR)];
    SP_DEVICE_INTERFACE_DETAIL_DATA_W *detail = (void *)buffer;

    LOG("scanning devices\n");
    if (set == INVALID_HANDLE_VALUE) return;
    for (DWORD idx = 0; SetupDiEnumDeviceInterfaces(set, NULL, &hid_guid, idx, &iface); idx++)
    {
        int port;
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, sizeof(buffer), NULL, NULL)) continue;
        LOG("HID %ls\n", detail->DevicePath);
        if (!path_is_twin(detail->DevicePath, &port)) continue;
        struct pad *p = &pads[port];
        if (p->connected) continue;
        if (p->handle && p->handle != INVALID_HANDLE_VALUE) CloseHandle(p->handle);
        if (p->preparsed) { HidD_FreePreparsedData(p->preparsed); p->preparsed = NULL; }
        p->handle = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
        if (p->handle == INVALID_HANDLE_VALUE)
        {
            LOG("port %d: CreateFile failed %lu\n", port + 1, GetLastError());
            p->handle = NULL;
            continue;
        }
        if (!HidD_GetPreparsedData(p->handle, &p->preparsed) ||
            HidP_GetCaps(p->preparsed, &p->caps) != HIDP_STATUS_SUCCESS)
        {
            LOG("port %d: preparsed/caps failed %lu\n", port + 1, GetLastError());
            CloseHandle(p->handle);
            p->handle = NULL;
            continue;
        }
        LOG("port %d connected: report %u B, button caps %u, value caps %u\n", port + 1,
            p->caps.InputReportByteLength, p->caps.NumberInputButtonCaps, p->caps.NumberInputValueCaps);
        HidD_SetNumInputBuffers(p->handle, 64);
        setup_axes(p);
        memset(&p->state, 0, sizeof(p->state));
        p->connected = TRUE;
        CloseHandle(CreateThread(NULL, 0, reader_thread, p, 0, NULL));
    }
    SetupDiDestroyDeviceInfoList(set);
}

static struct pad *get_pad(DWORD index)
{
    InitOnceExecuteOnce(&init_once, do_init, NULL, NULL);
    if (index >= XUSER_MAX_COUNT) return NULL;
    if (!pads[0].connected || !pads[1].connected)
    {
        DWORD now = GetTickCount();
        EnterCriticalSection(&scan_lock);
        if (!last_scan || now - last_scan >= 2000) { last_scan = now ? now : 1; scan_devices(); }
        LeaveCriticalSection(&scan_lock);
    }
    if (!pads[index].connected) return NULL;
    return &pads[index];
}

__declspec(dllexport) DWORD WINAPI XInputGetState(DWORD index, XINPUT_STATE *state)
{
    struct pad *p = get_pad(index);
    if (!p) return ERROR_DEVICE_NOT_CONNECTED;
    if (!state) return ERROR_BAD_ARGUMENTS;
    EnterCriticalSection(&p->lock);
    *state = p->state;
    LeaveCriticalSection(&p->lock);
    if (!enabled) memset(&state->Gamepad, 0, sizeof(state->Gamepad));
    return ERROR_SUCCESS;
}

/* ordinal 100 (unnamed) in xinput1_3/1_4: same as GetState, there is no Guide button */
__declspec(dllexport) DWORD WINAPI XInputGetStateEx(DWORD index, XINPUT_STATE *state)
{
    return XInputGetState(index, state);
}

__declspec(dllexport) DWORD WINAPI XInputSetState(DWORD index, XINPUT_VIBRATION *vib)
{
    return get_pad(index) ? ERROR_SUCCESS : ERROR_DEVICE_NOT_CONNECTED;
}

__declspec(dllexport) DWORD WINAPI XInputGetCapabilities(DWORD index, DWORD flags, XINPUT_CAPABILITIES *caps)
{
    if (!get_pad(index)) return ERROR_DEVICE_NOT_CONNECTED;
    if (!caps) return ERROR_BAD_ARGUMENTS;
    memset(caps, 0, sizeof(*caps));
    caps->Type = 1;      /* XINPUT_DEVTYPE_GAMEPAD */
    caps->SubType = 1;   /* XINPUT_DEVSUBTYPE_GAMEPAD */
    caps->Gamepad.wButtons = 0xf3ff;
    caps->Gamepad.bLeftTrigger = caps->Gamepad.bRightTrigger = 255;
    caps->Gamepad.sThumbLX = caps->Gamepad.sThumbLY = caps->Gamepad.sThumbRX = caps->Gamepad.sThumbRY = (SHORT)0xffc0;
    return ERROR_SUCCESS;
}

__declspec(dllexport) void WINAPI XInputEnable(BOOL enable)
{
    enabled = enable;
}

__declspec(dllexport) DWORD WINAPI XInputGetDSoundAudioDeviceGuids(DWORD index, GUID *render, GUID *capture)
{
    if (!get_pad(index)) return ERROR_DEVICE_NOT_CONNECTED;
    if (render) memset(render, 0, sizeof(*render));
    if (capture) memset(capture, 0, sizeof(*capture));
    return ERROR_SUCCESS;
}

__declspec(dllexport) DWORD WINAPI XInputGetBatteryInformation(DWORD index, BYTE type, XINPUT_BATTERY_INFORMATION *info)
{
    if (!get_pad(index)) return ERROR_DEVICE_NOT_CONNECTED;
    if (info) { info->BatteryType = 1; info->BatteryLevel = 3; }   /* wired, full */
    return ERROR_SUCCESS;
}

__declspec(dllexport) DWORD WINAPI XInputGetKeystroke(DWORD index, DWORD reserved, XINPUT_KEYSTROKE *ks)
{
    return ERROR_EMPTY;
}

/* Games (e.g. UE5) confine the cursor with ClipCursor -> it can't reach the 2nd screen.
   Patch user32!ClipCursor & win32u!NtUserClipCursor to "return TRUE" (release the confinement).
   TWIN_ALLOW_CLIP=1 -> leave them original. */
static void patch_ret_true(const char *dll, const char *func)
{
    static const BYTE code[] = { 0xb8, 0x01, 0x00, 0x00, 0x00, 0xc3 };   /* mov eax,1 ; ret */
    HMODULE mod = GetModuleHandleA(dll);
    BYTE *fn = mod ? (BYTE *)GetProcAddress(mod, func) : NULL;
    DWORD old;
    if (!fn || !VirtualProtect(fn, sizeof(code), PAGE_EXECUTE_READWRITE, &old)) return;
    memcpy(fn, code, sizeof(code));
    VirtualProtect(fn, sizeof(code), old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, sizeof(code));
}

static void unclip_cursor(void)
{
    char v[8];
    if (GetEnvironmentVariableA("TWIN_ALLOW_CLIP", v, sizeof(v)) && v[0] == '1') return;
    patch_ret_true("win32u.dll", "NtUserClipCursor");
    patch_ret_true("user32.dll", "ClipCursor");
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, void *reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(inst);
        unclip_cursor();
    }
    return TRUE;
}
