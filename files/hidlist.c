#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <stdio.h>
int main(void) {
    GUID g; HidD_GetHidGuid(&g);
    HDEVINFO set = SetupDiGetClassDevsW(&g, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    SP_DEVICE_INTERFACE_DATA i = {sizeof(i)};
    char buf[4096]; SP_DEVICE_INTERFACE_DETAIL_DATA_W *d = (void *)buf;
    for (DWORD n = 0; SetupDiEnumDeviceInterfaces(set, NULL, &g, n, &i); n++) {
        d->cbSize = sizeof(*d);
        if (SetupDiGetDeviceInterfaceDetailW(set, &i, d, sizeof(buf), NULL, NULL)) printf("%ls\n", d->DevicePath);
    }
    printf("selesai, err=%lu\n", GetLastError());
    return 0;
}
