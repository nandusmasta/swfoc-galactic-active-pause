/* Offline checks against the real StarWarsG.exe image and Patch2.meg (no game running).
 * Build: gcc -O1 -o test_offline.exe test/test_offline.c src/winmm_proxy.c
 * Run:   test_offline.exe <corruption dir> <scratch dir> */
#include "../src/spacebar_pause.c"

static int failures;
#define CHECK(cond, ...) do { if (cond) printf("PASS "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static int stub_count, stub_speed;
static int stub_locked(int count, int speed)
{
    stub_count = count;
    stub_speed = speed;
    g_pause_count = &stub_count;
    g_speed_state = &stub_speed;
    unsigned char g;
    __asm__ volatile("call order_check_stub\n\tsetg %0" : "=m"(g) :: "cc", "memory");
    return g;
}

static char *read_all(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    _fseeki64(f, 0, SEEK_END);
    *size = (size_t)_ftelli64(f);
    _fseeki64(f, 0, SEEK_SET);
    char *b = malloc(*size + 1);
    fread(b, 1, *size, f);
    b[*size] = 0;
    fclose(f);
    return b;
}

int main(int argc, char **argv)
{
    char exe[MAX_PATH];
    snprintf(exe, sizeof exe, "%s\\StarWarsG.exe", argv[1]);
    g_base = (uint8_t *)LoadLibraryExA(exe, NULL, DONT_RESOLVE_DLL_REFERENCES);
    CHECK(g_base != NULL, "map StarWarsG.exe");
    CHECK(is_supported_build(), "build is 0x%X", BUILD_TIMESTAMP);
    CHECK(sites_match(), "all patch sites match");

    g_near = alloc_near_image(0x3000);
    CHECK(g_near && (intptr_t)(g_near - g_base) < 0x7fffffff && (intptr_t)(g_near - g_base) > -0x7fffffff, "near memory in rel32 reach");
    install_pause_key();
    install_order_patches();

    CHECK(g_base[0x2119ab] == 0xD0 && g_base[0x2119d0] == 0x9A && g_base[0x212e82] == 0x9A, "slot count patched to 154");
    int ok = 1;
    for (size_t i = 0; i < sizeof g_label_leas / sizeof g_label_leas[0]; i++) {
        uint8_t *lea = g_base + g_label_leas[i].lea;
        if (lea + 7 + *(int32_t *)(lea + 3) != g_near + 0x100) ok = 0;
    }
    CHECK(ok, "10 label-table references point at the extended table");
    CHECK(strcmp((char *)g_near + 0x100 + 74 * 64, "TEXT_CAMERA_CENTER") == 0, "copied labels intact (slot 74)");
    CHECK(strcmp((char *)g_near + 0x100 + PAUSE_SLOT * 64, PAUSE_LABEL) == 0, "slot 153 label is %s", PAUSE_LABEL);
    uint8_t *lea = g_base + RVA_TAB4_EVENT_LEA;
    uint8_t *tramp = lea + 7 + *(int32_t *)(lea + 3);
    CHECK(tramp == g_near + 0x20 && *(void **)(tramp + 6) == (void *)tab4_event_hook, "tab event callback -> tab4_event_hook");
    CHECK(g_base[RVA_HOTKEY_DISPATCH] == 0x48 && g_base[RVA_HOTKEY_DISPATCH + 1] == 0xB8 &&
          *(void **)(g_base + RVA_HOTKEY_DISPATCH + 2) == (void *)hotkey_dispatch_hook, "dispatcher detoured");
    CHECK(*(void **)(g_base + RVA_COMMAND_BAR_PAUSE + 2) == (void *)pause_hook && g_base[RVA_COMMAND_BAR_PAUSE + 12] == 0x90,
          "Pause() detoured, stolen tail NOPed");
    CHECK(memcmp(g_original_pause, D_PAUSE.bytes, 13) == 0 && *(void **)((uint8_t *)g_original_pause + 19) ==
          g_base + RVA_COMMAND_BAR_PAUSE + 13, "Pause() trampoline returns after stolen bytes");
    CHECK(g_base[0x3c4495] == 0xE8 && g_base[0x3c4495 + 5] == 0x90, "order check site patched");
    CHECK(memcmp(g_base + 0x4b2366, "\x83\x3D\x9F\x30\x66\x00\x00", 7) == 0,
          "Is_Locked untouched (galactic time step must stay frozen while paused)");

    CHECK(!stub_locked(1, 0) && stub_locked(2, 0) && stub_locked(1, 1) && !stub_locked(0, 1), "order stub semantics");

    /* Dialog layout: generate into a scratch game dir holding a copy of Patch2.meg. */
    char dir[MAX_PATH], rc_path[MAX_PATH + 64];
    snprintf(dir, sizeof dir, "%s\\", argv[2]);
    snprintf(rc_path, sizeof rc_path, "%sData\\Resources\\GUIDialog\\GUIDialogs.rc", dir);
    DeleteFileA(rc_path);
    write_dialog_layout(dir);
    size_t n = 0;
    char *rc = read_all(rc_path, &n);
    CHECK(rc != NULL, "GUIDialogs.rc written");
    char *d4 = rc ? strstr(rc, "IDD_KEYBOARD_OPTIONS_DIALOG_4 DIALOGEX") : NULL;
    char *row = rc ? strstr(rc, PAUSE_CONTROL) : NULL;
    char *d4_end = d4 ? strstr(d4, "\nEND") : NULL;
    CHECK(d4 && row && row > d4 && row < d4_end, "Pause/Play row inside the Interface tab dialog");
    CHECK(rc && strstr(rc, "\"" PAUSE_LABEL "\",IDC_STATIC,460,290,235,23\r\n"), "label line present with CRLF");
    CHECK(d4_end && strncmp(d4_end, "\nEND", 4) == 0 && d4_end[-1] == '\r', "dialog block still ends with CRLF END");

    FILE *f = fopen(rc_path, "wb"); /* another mod's file must be left alone */
    fputs("foreign", f);
    fclose(f);
    write_dialog_layout(dir);
    free(rc);
    rc = read_all(rc_path, &n);
    CHECK(rc && strcmp(rc, "foreign") == 0, "foreign GUIDialogs.rc untouched");
    DeleteFileA(rc_path);

    printf("%d failure(s)\n", failures);
    return failures;
}
