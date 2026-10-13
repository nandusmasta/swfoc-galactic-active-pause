/*
 * Space Bar Pause for Star Wars: Empire at War - Forces of Corruption (Steam 64-bit).
 *
 * Built as a proxy winmm.dll that StarWarsG.exe loads from its own folder. On load it
 * detours the game's hotkey dispatcher so that pressing Space on the galactic map does
 * exactly what clicking the Pause/Play button does.
 *
 * All addresses are RVAs into StarWarsG.exe (PE timestamp 0x67002393). Every patch site
 * and every game function we call is checked against a byte signature first; on any
 * mismatch (e.g. after a game update) nothing is patched and the game runs unmodified.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MOD_VERSION "1.0.0"

BOOL proxy_init(void);

/* ---- Game layout (RVAs) ------------------------------------------------------------ */

/* char Hotkey_Dispatch(int vk) - looks up the key binding and runs the hotkey command. */
#define RVA_HOTKEY_DISPATCH     0x063f00
#define HOOK_LEN                12
/* char GUIDialogManager::Is_Modal_Active(this) and its singleton. */
#define RVA_DIALOG_IS_ACTIVE    0x1c3c40
#define RVA_DIALOG_MANAGER      0xa78510
/* Byte the dispatcher checks before handling any hotkey (input blocked). */
#define RVA_INPUT_BLOCKED       0xb155c4
/* GameModeClass *current mode; vfunc Get_Mode_Type: 0 galactic, 1/2 land/space tactical. */
#define RVA_CURRENT_MODE        0xb15418
#define VT_GET_MODE_TYPE        (0xe0 / 8)
/* GameModeManager+0x2c: galactic pause count. CommandBar::Pause/Play add/remove one; pending
 * battles and story scripts add their own. Galactic time is frozen while it is > 0. */
#define RVA_PAUSE_COUNT         0xb1540c
/* CommandBarClass singleton, its Pause()/Play() (the b_play_pause button handlers). */
#define RVA_COMMAND_BAR         0xb27f60
#define RVA_COMMAND_BAR_PAUSE   0x31f870
#define RVA_COMMAND_BAR_PLAY    0x31fcd0
#define CB_OFS_SPEED_STATE      0x2968   /* int: 0 paused, 1 normal, 2 fast */
#define CB_OFS_LAYOUT           0x211c   /* int: 1 galactic command bar */
/* Command bar component table: 0x48-byte entries, component pointer at +8. */
#define RVA_COMPONENT_TABLE     0xa174e0
#define COMPONENT_STRIDE        0x48
#define COMPONENT_B_PLAY_PAUSE  0x26a
#define VT_BUTTON_SET_STATE     (0x1d0 / 8)   /* 3 = toggled on, 1 = normal */

/*
 * Places that refuse player orders while the galactic pause count is > 0. Each is a
 * `cmp dword [pause_count], 0` (the reg forms compare against eax, which is 0 there).
 * For "orders while paused" they are redirected to a stub that compares the count minus
 * the player's own Pause-button pause, so orders go through during a manual pause but
 * not while a battle is pending or a script holds the pause. The galactic object update
 * (which actually freezes time) still reads the real count.
 */
struct order_check_site {
    uint32_t rva;
    const char *what;
    uint8_t len;
    uint8_t bytes[7];
};

static const struct order_check_site g_order_sites[] = {
    { 0x3c4495, "MoveThroughObjectsEvent::Execute (fleet move)", 6, { 0x39, 0x05, 0x71, 0x0F, 0x75, 0x00 } },
    { 0x5af297, "FleetManagementEvent::Execute (orbit/surface)", 6, { 0x39, 0x05, 0x6F, 0x61, 0x56, 0x00 } },
    { 0x3acc12, "SelectEvent::Execute",                          6, { 0x39, 0x05, 0xF4, 0x87, 0x76, 0x00 } },
    { 0x4b2366, "GalacticModeClass::Is_Locked",                  7, { 0x83, 0x3D, 0x9F, 0x30, 0x66, 0x00, 0x00 } },
    { 0x03beaf, "galactic selection order input",                7, { 0x83, 0x3D, 0x56, 0x95, 0xAD, 0x00, 0x00 } },
};

struct signature {
    uint32_t rva;
    const char *what;
    size_t len;
    const uint8_t *bytes;
};

#define SIG(rva, what, ...) \
    { rva, what, sizeof((const uint8_t[]){__VA_ARGS__}), (const uint8_t[]){__VA_ARGS__} }

static const struct signature g_signatures[] = {
    /* mov [rsp+8],rbx; push rbp; push rsi; push rdi; sub rsp,40h; mov ebx,ecx */
    SIG(RVA_HOTKEY_DISPATCH, "hotkey dispatcher",
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x55, 0x56, 0x57, 0x48, 0x83, 0xEC, 0x40, 0x8B, 0xD9),
    /* ...; cmp dword [rcx+2968h],0 */
    SIG(RVA_COMMAND_BAR_PAUSE, "CommandBar::Pause",
        0x48, 0x89, 0x5C, 0x24, 0x18, 0x55, 0x48, 0x8B, 0xEC, 0x48, 0x83, 0xEC, 0x60,
        0x83, 0xB9, 0x68, 0x29, 0x00, 0x00, 0x00),
    /* push rbx; sub rsp,40h; cmp dword [rcx+2968h],1 */
    SIG(RVA_COMMAND_BAR_PLAY, "CommandBar::Play",
        0x40, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x83, 0xB9, 0x68, 0x29, 0x00, 0x00, 0x01),
    /* cmp qword [rcx+98h],0; setne al; ret */
    SIG(RVA_DIALOG_IS_ACTIVE, "DialogManager::Is_Active",
        0x48, 0x83, 0xB9, 0x98, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x95, 0xC0, 0xC3),
    /* b_play_pause handlers: lea rcx,[CommandBar]; jmp Pause / jmp Play */
    SIG(0x3477b0, "b_play_pause click thunk",
        0x48, 0x8D, 0x0D, 0xA9, 0x07, 0x7E, 0x00, 0xE9, 0xB4, 0x80, 0xFD, 0xFF),
    SIG(0x3477c0, "b_play_pause unclick thunk",
        0x48, 0x8D, 0x0D, 0x99, 0x07, 0x7E, 0x00, 0xE9, 0x04, 0x85, 0xFD, 0xFF),
    /* Get_Component(id): movsxd; lea rcx,[table+8]; lea rax,[rax+rax*8]; mov rax,[rcx+rax*8] */
    SIG(0x347b50, "Get_Component",
        0x48, 0x63, 0xC1, 0x48, 0x8D, 0x0D, 0x8E, 0xF9, 0x6C, 0x00,
        0x48, 0x8D, 0x04, 0xC0, 0x48, 0x8B, 0x04, 0xC1, 0xC3),
    /* The game's own programmatic pause: button->vfunc[1D0h](3); CommandBar::Pause() */
    SIG(0x2fdc12, "game pause call site",
        0xBA, 0x03, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xC8, 0x41, 0xFF, 0x90, 0xD0, 0x01, 0x00, 0x00),
    /* Dispatcher's input guards (validates the globals we read). */
    SIG(0x063f13, "dispatcher guards",
        0x48, 0x8D, 0x0D, 0xF6, 0x45, 0xA1, 0x00,        /* lea rcx,[DialogManager] */
        0xE8, 0x21, 0xFD, 0x15, 0x00,                    /* call Is_Active */
        0x3C, 0x01, 0x0F, 0x84, 0xC8, 0x12, 0x00, 0x00,  /* cmp al,1; je out */
        0x48, 0x83, 0x3D, 0xE9, 0x14, 0xAB, 0x00, 0x00,  /* cmp [CurrentMode],0 */
        0x0F, 0x84, 0xBA, 0x12, 0x00, 0x00,              /* je out */
        0x83, 0xFB, 0x1B, 0x0F, 0x84, 0xB1, 0x12, 0x00, 0x00, /* cmp ebx,VK_ESCAPE; je out */
        0x80, 0x3D, 0x7F, 0x16, 0xAB, 0x00, 0x00),       /* cmp byte [InputBlocked],0 */
};

/* ---- State ------------------------------------------------------------------------- */

typedef char (*HotkeyDispatchFn)(int vk);
typedef char (*IsActiveFn)(void *self);
typedef void (*CommandBarFn)(void *self);
typedef int (*GetModeTypeFn)(void *self);
typedef void (*SetStateFn)(void *self, int state);

static uint8_t *g_base;
static HotkeyDispatchFn g_original_dispatch;
static int g_pause_key = VK_SPACE;
static char g_log_path[MAX_PATH + 32];

static void log_line(const char *fmt, ...)
{
    if (!g_log_path[0]) return;
    FILE *f = fopen(g_log_path, "a");
    if (!f) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

#define GAME(type, rva) ((type)(g_base + (rva)))

static void *vfunc(void *obj, int index)
{
    return (*(void ***)obj)[index];
}

/* Mirrors a click on the galactic Pause/Play button. Returns nonzero if the key was used. */
static int toggle_galactic_pause(void)
{
    if (GetKeyState(VK_SHIFT) < 0 || GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_MENU) < 0)
        return 0;
    if (GAME(IsActiveFn, RVA_DIALOG_IS_ACTIVE)(g_base + RVA_DIALOG_MANAGER))
        return 0;
    if (*GAME(uint8_t *, RVA_INPUT_BLOCKED))
        return 0;

    void *mode = *GAME(void **, RVA_CURRENT_MODE);
    if (!mode || ((GetModeTypeFn)vfunc(mode, VT_GET_MODE_TYPE))(mode) != 0)
        return 0;
    /* Pause()/Play() carry the game's own checks (multiplayer, pending battle, ...). */

    uint8_t *command_bar = g_base + RVA_COMMAND_BAR;
    if (*(int *)(command_bar + CB_OFS_LAYOUT) != 1)
        return 0;

    int before = *(int *)(command_bar + CB_OFS_SPEED_STATE);
    if (before == 0)
        GAME(CommandBarFn, RVA_COMMAND_BAR_PLAY)(command_bar);
    else
        GAME(CommandBarFn, RVA_COMMAND_BAR_PAUSE)(command_bar);
    int after = *(int *)(command_bar + CB_OFS_SPEED_STATE);
    if (after == before)
        return 0; /* the game refused (multiplayer, story lock, ...) */

    /* Keep the button's toggle state in sync, as a mouse click would. */
    void *button = *GAME(void **, RVA_COMPONENT_TABLE + COMPONENT_B_PLAY_PAUSE * COMPONENT_STRIDE + 8);
    if (button)
        ((SetStateFn)vfunc(button, VT_BUTTON_SET_STATE))(button, after == 0 ? 3 : 1);
    return 1;
}

static char hotkey_dispatch_hook(int vk)
{
    if (vk == g_pause_key && toggle_galactic_pause())
        return 1;
    return g_original_dispatch(vk);
}

/* Read by order_check_stub. */
int *g_pause_count;
int *g_speed_state;

/*
 * Called in place of `cmp dword [pause_count], 0`. Sets the flags as if comparing
 * (pause_count - manual pause) with 0, where the manual pause is the one the Pause
 * button added (CommandBar speed state 0). Preserves every register.
 */
__attribute__((naked)) void order_check_stub(void)
{
    __asm__ volatile(
        "push %rcx\n\t"
        "push %rdx\n\t"
        "mov  g_pause_count(%rip), %rcx\n\t"
        "mov  (%rcx), %ecx\n\t"
        "mov  g_speed_state(%rip), %rdx\n\t"
        "cmpl $0, (%rdx)\n\t"
        "jne  1f\n\t"
        "test %ecx, %ecx\n\t"
        "jle  1f\n\t"
        "dec  %ecx\n\t"
        "1:\n\t"
        "pop  %rdx\n\t"
        "cmp  $0, %ecx\n\t"
        "pop  %rcx\n\t" /* pop leaves the flags alone */
        "ret\n\t");
}

/* ---- Installation ------------------------------------------------------------------ */

static int signatures_match(void)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)g_base;
    const IMAGE_NT_HEADERS64 *nt = (const IMAGE_NT_HEADERS64 *)(g_base + dos->e_lfanew);
    DWORD image_size = nt->OptionalHeader.SizeOfImage;

    int ok = 1;
    for (size_t i = 0; i < sizeof g_signatures / sizeof g_signatures[0]; i++) {
        const struct signature *s = &g_signatures[i];
        if (s->rva + s->len > image_size) {
            log_line("Signature out of range: %s (RVA 0x%06X).", s->what, s->rva);
            return 0;
        }
        if (memcmp(g_base + s->rva, s->bytes, s->len) != 0) {
            log_line("Signature mismatch: %s (RVA 0x%06X).", s->what, s->rva);
            ok = 0;
        }
    }
    return ok;
}

static int install_hook(void)
{
    uint8_t *target = g_base + RVA_HOTKEY_DISPATCH;

    /* Trampoline: the 12 stolen bytes (no RIP-relative code) then jmp back. */
    uint8_t *tramp = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return 0;
    memcpy(tramp, target, HOOK_LEN);
    tramp[HOOK_LEN + 0] = 0xFF; /* jmp [rip+0] */
    tramp[HOOK_LEN + 1] = 0x25;
    memset(tramp + HOOK_LEN + 2, 0, 4);
    uint64_t back = (uint64_t)(target + HOOK_LEN);
    memcpy(tramp + HOOK_LEN + 6, &back, 8);
    g_original_dispatch = (HotkeyDispatchFn)tramp;

    /* mov rax, imm64; jmp rax */
    uint8_t patch[HOOK_LEN] = { 0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xE0 };
    uint64_t hook = (uint64_t)&hotkey_dispatch_hook;
    memcpy(patch + 2, &hook, 8);

    DWORD old;
    if (!VirtualProtect(target, HOOK_LEN, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy(target, patch, HOOK_LEN);
    VirtualProtect(target, HOOK_LEN, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, HOOK_LEN);
    return 1;
}

/* Executable memory within rel32 reach of the game image. */
static uint8_t *alloc_near_image(void)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)g_base;
    const IMAGE_NT_HEADERS64 *nt = (const IMAGE_NT_HEADERS64 *)(g_base + dos->e_lfanew);
    uintptr_t start = ((uintptr_t)g_base + nt->OptionalHeader.SizeOfImage + 0xFFFF) & ~(uintptr_t)0xFFFF;
    for (uintptr_t a = start; a < (uintptr_t)g_base + 0x70000000; a += 0x10000) {
        void *p = VirtualAlloc((void *)a, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (p) return p;
    }
    for (uintptr_t a = ((uintptr_t)g_base - 0x10000) & ~(uintptr_t)0xFFFF;
         a > (uintptr_t)g_base - 0x70000000 && a > 0x10000; a -= 0x10000) {
        void *p = VirtualAlloc((void *)a, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (p) return p;
    }
    return NULL;
}

static int install_order_patches(void)
{
    size_t count = sizeof g_order_sites / sizeof g_order_sites[0];
    for (size_t i = 0; i < count; i++) {
        const struct order_check_site *s = &g_order_sites[i];
        if (memcmp(g_base + s->rva, s->bytes, s->len) != 0) {
            log_line("Signature mismatch: %s (RVA 0x%06X); orders-while-paused disabled.", s->what, s->rva);
            return 0;
        }
    }

    g_pause_count = GAME(int *, RVA_PAUSE_COUNT);
    g_speed_state = (int *)(g_base + RVA_COMMAND_BAR + CB_OFS_SPEED_STATE);

    uint8_t *tramp = alloc_near_image();
    if (!tramp) {
        log_line("No memory near the game image; orders-while-paused disabled.");
        return 0;
    }
    tramp[0] = 0xFF; /* jmp [rip+0] -> order_check_stub */
    tramp[1] = 0x25;
    memset(tramp + 2, 0, 4);
    uint64_t stub = (uint64_t)&order_check_stub;
    memcpy(tramp + 6, &stub, 8);

    for (size_t i = 0; i < count; i++) {
        const struct order_check_site *s = &g_order_sites[i];
        uint8_t *site = g_base + s->rva;
        uint8_t patch[7];
        int32_t rel = (int32_t)((intptr_t)tramp - (intptr_t)(site + 5));
        patch[0] = 0xE8; /* call rel32 */
        memcpy(patch + 1, &rel, 4);
        memset(patch + 5, 0x90, s->len - 5);

        DWORD old;
        if (!VirtualProtect(site, s->len, PAGE_EXECUTE_READWRITE, &old)) return 0;
        memcpy(site, patch, s->len);
        VirtualProtect(site, s->len, old, &old);
        FlushInstructionCache(GetCurrentProcess(), site, s->len);
    }
    return 1;
}

static void setup(HMODULE self)
{
    char exe[MAX_PATH];
    GetModuleFileNameA(NULL, exe, MAX_PATH);
    const char *exe_name = strrchr(exe, '\\') ? strrchr(exe, '\\') + 1 : exe;
    if (lstrcmpiA(exe_name, "StarWarsG.exe") != 0)
        return; /* the launcher, or another game: just forward winmm */

    char dir[MAX_PATH];
    GetModuleFileNameA(self, dir, MAX_PATH);
    char *slash = strrchr(dir, '\\');
    if (slash) slash[1] = '\0';
    snprintf(g_log_path, sizeof g_log_path, "%sSpaceBarPause.log", dir);

    char ini[MAX_PATH + 32];
    snprintf(ini, sizeof ini, "%sSpaceBarPause.ini", dir);
    g_pause_key = GetPrivateProfileIntA("SpaceBarPause", "Key", VK_SPACE, ini);
    int allow_orders = GetPrivateProfileIntA("SpaceBarPause", "AllowOrdersWhilePaused", 1, ini);

    g_base = (uint8_t *)GetModuleHandleA(NULL);
    if (!signatures_match()) {
        log_line("Unsupported StarWarsG.exe build; Space bar pause disabled, game left unmodified.");
        return;
    }
    if (install_hook())
        log_line("Space Bar Pause v" MOD_VERSION " active (key 0x%02X) on galactic map.", g_pause_key);
    else
        log_line("Failed to install hook (error %lu).", GetLastError());

    if (allow_orders && install_order_patches())
        log_line("Orders while paused enabled (%u checks relaxed).",
                 (unsigned)(sizeof g_order_sites / sizeof g_order_sites[0]));
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        if (!proxy_init())
            return FALSE;
        setup(inst);
    }
    return TRUE;
}
