/*
 * Space Bar Pause for Star Wars: Empire at War - Forces of Corruption (Steam 64-bit).
 *
 * Built as a proxy winmm.dll that StarWarsG.exe loads from its own folder. It adds a
 * rebindable "Pause/Play" key (key slot 153, default Pause) that toggles the galactic
 * pause exactly like the Pause/Play button, removes the pause banner animation on the
 * galactic map, and lets the player give orders while they have the game paused.
 *
 * All addresses are RVAs into StarWarsG.exe build 0x67002393. Anything else is left
 * alone, and every byte we overwrite is checked first.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOD_VERSION "1.1.0"

BOOL proxy_init(void);

/* ---- Game layout (RVAs) ------------------------------------------------------------ */

#define BUILD_TIMESTAMP         0x67002393
#define BUILD_IMAGE_SIZE        0xcc0000

/* char Hotkey_Dispatch(int vk) - looks up the key binding and runs the hotkey command. */
#define RVA_HOTKEY_DISPATCH     0x063f00
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
#define COMPONENT_PAUSE_SHELL   0x2ab    /* animated "Game Paused" banner */
#define VT_SET_HIDDEN           (0x78 / 8)
#define VT_BUTTON_SET_STATE     (0x1d0 / 8)   /* 3 = toggled on, 1 = normal */

/* Key bindings: { int key; int modifier; }[count] at +8, count at +0x10. 153 slots in the
 * game; we add slot 153. The real table and the options dialog's working copy: */
#define PAUSE_SLOT              153
#define RVA_KEYS                0xa7c3a8
#define RVA_KEYS_WORK           0xa7c3c0
#define RVA_KEYS_DEFAULTS       0x211940   /* void Set_Defaults(keys) */
#define RVA_KEYS_SET            0x212520   /* void Set(keys, slot, key, mod) */
#define RVA_KEYS_GET            0x2126b0   /* pair *Get(keys, slot) */
#define RVA_KEYS_SET_PAIR       0x212420   /* void Set(keys, slot, pair *) */
#define RVA_KEYS_LOOKUP         0x2127f0   /* int Lookup(keys, vk), uses modifier state */

/* Keyboard options, Interface tab. */
#define RVA_DIALOG_ID           0xa11610   /* int: dialog currently shown */
#define RVA_TAB4_REFRESH        0x0aefa0
#define RVA_TAB4_EVENT          0x0aeaa0   /* registered as a callback at RVA_TAB4_EVENT_LEA */
#define RVA_TAB4_EVENT_LEA      0x0ae546
#define RVA_TAB4_BIND           0x0af410   /* void Bind(slot, pair *) - warns on duplicates */
#define RVA_DLG_IS_SHOWN        0x1c1e90   /* char (mgr, dialog) */
#define RVA_DLG_ENABLE          0x1c24e0   /* (mgr, dialog, string *name, 1, 1, 1) */
#define RVA_DLG_SET_TEXT        0x1c2760   /* (mgr, dialog, string *name, wstring *text) */
#define RVA_DLG_HAS_FOCUS       0x1c2b10   /* char (mgr, dialog, string *name) */
#define RVA_DLG_SET_FOCUS       0x1c2d70   /* (mgr, dialog, string *name, char focus) */
#define RVA_KEY_TO_TEXT         0x0aa720   /* char (pair *, wstring *out) */
#define RVA_STRING_ASSIGN       0x022730   /* (string *, const char *, size_t) */
#define RVA_DELETE              0x769c94   /* operator delete(void *, size_t) */
#define PAUSE_CONTROL           "IDC_EDIT_GAME_OPTIONS"   /* defined in Resource.h, unused */
#define PAUSE_LABEL             "TEXT_TOOLTIP_PAUSE_PLAY_NAME"   /* "Pause/Play" */

/* Per-slot command names (64 bytes each, 123 used) shown in the duplicate-key warning.
 * Each keyboard tab references its own identical copy twice; we point all ten at one
 * copy that also names slot 153. */
#define RVA_LABEL_TABLE         0x8103d0
static const struct { uint32_t lea, table; } g_label_leas[] = {
    { 0x0ab1d8, 0x807330 }, { 0x0ab269, 0x807330 }, { 0x0ac86b, 0x80a730 }, { 0x0ac8fc, 0x80a730 },
    { 0x0adf0b, 0x80d580 }, { 0x0adf9c, 0x80d580 }, { 0x0af5ab, 0x8103d0 }, { 0x0af63c, 0x8103d0 },
    { 0x0b0c4b, 0x813220 }, { 0x0b0cdc, 0x813220 },
};

/* Constants that size the key table at 153 slots, raised to 154. */
struct imm_patch {
    uint32_t rva;
    uint8_t len;
    uint8_t old[6];
    uint8_t new[6];
};
static const struct imm_patch g_slot_count_patches[] = {
    { 0x2119a9, 6, { 0x41, 0xB8, 0xC8, 0x04, 0x00, 0x00 }, { 0x41, 0xB8, 0xD0, 0x04, 0x00, 0x00 } }, /* alloc 153*8 */
    { 0x2119bd, 5, { 0xB9, 0xC8, 0x04, 0x00, 0x00 }, { 0xB9, 0xD0, 0x04, 0x00, 0x00 } },             /* alloc 153*8 */
    { 0x2119cf, 5, { 0xB9, 0x99, 0x00, 0x00, 0x00 }, { 0xB9, 0x9A, 0x00, 0x00, 0x00 } },             /* clear loop */
    { 0x211a07, 5, { 0x25, 0x99, 0x00, 0x00, 0x80 }, { 0x25, 0x9A, 0x00, 0x00, 0x80 } },             /* capacity */
    { 0x211a0c, 5, { 0x0D, 0x99, 0x00, 0x00, 0x00 }, { 0x0D, 0x9A, 0x00, 0x00, 0x00 } },             /* count */
    { 0x212e81, 5, { 0xBB, 0x99, 0x00, 0x00, 0x00 }, { 0xBB, 0x9A, 0x00, 0x00, 0x00 } },             /* registry load cap */
};

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

/* Function starts we detour: the stolen bytes hold no RIP-relative code. */
struct detour_site {
    uint32_t rva;
    uint8_t len;
    uint8_t bytes[15];
};
static const struct detour_site D_DISPATCH = { RVA_HOTKEY_DISPATCH, 12,
    { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x55, 0x56, 0x57, 0x48, 0x83, 0xEC, 0x40 } };
static const struct detour_site D_PAUSE = { RVA_COMMAND_BAR_PAUSE, 13,
    { 0x48, 0x89, 0x5C, 0x24, 0x18, 0x55, 0x48, 0x8B, 0xEC, 0x48, 0x83, 0xEC, 0x60 } };
static const struct detour_site D_DEFAULTS = { RVA_KEYS_DEFAULTS, 15,
    { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20 } };
static const struct detour_site D_TAB4_REFRESH = { RVA_TAB4_REFRESH, 15,
    { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x48, 0x89, 0x7C, 0x24, 0x18 } };

/* ---- State ------------------------------------------------------------------------- */

typedef char (*HotkeyDispatchFn)(int vk);
typedef char (*IsActiveFn)(void *self);
typedef void (*CommandBarFn)(void *self);
typedef int (*GetModeTypeFn)(void *self);
typedef void (*SetIntFn)(void *self, int value);
typedef void (*KeysFn)(void *keys);
typedef void (*KeysSetFn)(void *keys, int slot, int key, int mod);
typedef void *(*KeysGetFn)(void *keys, int slot);
typedef void (*KeysSetPairFn)(void *keys, int slot, void *pair);
typedef int (*KeysLookupFn)(void *keys, int vk);
typedef void (*VoidFn)(void);
typedef void (*EventFn)(void *event);
typedef void (*BindFn)(int slot, void *pair);
typedef char (*DlgIsShownFn)(void *mgr, int dialog);
typedef char (*DlgEnableFn)(void *mgr, int dialog, void *name, char a, char b, char c);
typedef void (*DlgSetTextFn)(void *mgr, int dialog, void *name, void *text);
typedef char (*DlgHasFocusFn)(void *mgr, int dialog, void *name);
typedef void (*DlgSetFocusFn)(void *mgr, int dialog, void *name, char focus);
typedef char (*KeyToTextFn)(void *pair, void *text);
typedef void (*StringAssignFn)(void *s, const char *text, size_t len);
typedef void (*DeleteFn)(void *p, size_t size);

/* MSVC std::string / std::wstring layout: inline buffer or pointer, then size, capacity. */
struct msvc_string {
    union { char buf[16]; void *ptr; } u;
    size_t size;
    size_t cap;
};

static uint8_t *g_base;
static HotkeyDispatchFn g_original_dispatch;
static CommandBarFn g_original_pause;
static KeysFn g_original_defaults;
static VoidFn g_original_tab4_refresh;
static uint8_t *g_near;   /* executable memory within rel32 reach of the game */
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

static void *component(int id)
{
    return *GAME(void **, RVA_COMPONENT_TABLE + id * COMPONENT_STRIDE + 8);
}

static int in_galactic_mode(void)
{
    void *mode = *GAME(void **, RVA_CURRENT_MODE);
    return mode && ((GetModeTypeFn)vfunc(mode, VT_GET_MODE_TYPE))(mode) == 0;
}

/* ---- Pause/Play hotkey -------------------------------------------------------------- */

/* Mirrors a click on the galactic Pause/Play button. Returns nonzero if the key was used. */
static int toggle_galactic_pause(void)
{
    if (GAME(IsActiveFn, RVA_DIALOG_IS_ACTIVE)(g_base + RVA_DIALOG_MANAGER))
        return 0;
    if (*GAME(uint8_t *, RVA_INPUT_BLOCKED))
        return 0;
    if (!in_galactic_mode())
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
    void *button = component(COMPONENT_B_PLAY_PAUSE);
    if (button)
        ((SetIntFn)vfunc(button, VT_BUTTON_SET_STATE))(button, after == 0 ? 3 : 1);
    return 1;
}

static char hotkey_dispatch_hook(int vk)
{
    if (GAME(KeysLookupFn, RVA_KEYS_LOOKUP)(g_base + RVA_KEYS, vk) == PAUSE_SLOT && toggle_galactic_pause())
        return 1;
    return g_original_dispatch(vk);
}

/* Pause() starts the "Game Paused" banner's slide-in animation; on the galactic map we
 * hide the banner straight away so pausing is instant. Play() already hides it. */
static void pause_hook(void *command_bar)
{
    g_original_pause(command_bar);
    void *shell = component(COMPONENT_PAUSE_SHELL);
    if (shell && in_galactic_mode())
        ((SetIntFn)vfunc(shell, VT_SET_HIDDEN))(shell, 1);
}

static void defaults_hook(void *keys)
{
    g_original_defaults(keys);
    GAME(KeysSetFn, RVA_KEYS_SET)(keys, PAUSE_SLOT, VK_PAUSE, 0);
}

/* ---- Keyboard options, Interface tab: the Pause/Play row --------------------------- */

static void free_game_string(struct msvc_string *s, size_t char_size, size_t inline_cap)
{
    if (s->cap < inline_cap) return;
    uint8_t *p = s->u.ptr;
    size_t bytes = (s->cap + 1) * char_size;
    if (bytes >= 0x1000) { /* MSVC big-allocation alignment header */
        p = *(uint8_t **)(p - 8);
        bytes += 0x27;
    }
    GAME(DeleteFn, RVA_DELETE)(p, bytes);
}

static void control_name(struct msvc_string *s)
{
    memset(s, 0, sizeof *s);
    s->cap = 15;
    GAME(StringAssignFn, RVA_STRING_ASSIGN)(s, PAUSE_CONTROL, sizeof PAUSE_CONTROL - 1);
}

static void tab4_refresh_hook(void)
{
    g_original_tab4_refresh();

    void *mgr = g_base + RVA_DIALOG_MANAGER;
    int dialog = *GAME(int *, RVA_DIALOG_ID);
    struct msvc_string name, text = { .cap = 7 };
    control_name(&name);
    GAME(DlgEnableFn, RVA_DLG_ENABLE)(mgr, dialog, &name, 1, 1, 1);
    void *pair = GAME(KeysGetFn, RVA_KEYS_GET)(g_base + RVA_KEYS_WORK, PAUSE_SLOT);
    if (GAME(KeyToTextFn, RVA_KEY_TO_TEXT)(pair, &text))
        GAME(DlgSetTextFn, RVA_DLG_SET_TEXT)(mgr, dialog, &name, &text);
    free_game_string(&text, 2, 8);
    free_game_string(&name, 1, 16);
}

/* Event: +0 type, +8 std::string control name, +0x2c key, +0x34 modifier. Handles our
 * control the way the tab's own loops handle theirs; returns 0 to let the game handle it. */
static int handle_pause_control(uint8_t *event)
{
    void *mgr = g_base + RVA_DIALOG_MANAGER;
    int dialog = *GAME(int *, RVA_DIALOG_ID);
    if (!GAME(DlgIsShownFn, RVA_DLG_IS_SHOWN)(mgr, dialog))
        return 0;

    int type = *(int *)event;
    if (type == 14) { /* a key was entered in an edit box */
        struct msvc_string *ev_name = (struct msvc_string *)(event + 8);
        const char *s = ev_name->cap >= 16 ? ev_name->u.ptr : ev_name->u.buf;
        if (ev_name->size != sizeof PAUSE_CONTROL - 1 || memcmp(s, PAUSE_CONTROL, ev_name->size) != 0)
            return 0;
        int pair[2] = { *(int *)(event + 0x2c), *(int *)(event + 0x34) };
        if ((unsigned)(pair[0] - VK_SHIFT) > 2) /* Shift/Ctrl/Alt alone are not bindings */
            GAME(BindFn, RVA_TAB4_BIND)(PAUSE_SLOT, pair);
        return 1;
    }
    if (type == 0) { /* cancel: revert the box being edited, or close the dialog */
        struct msvc_string name;
        control_name(&name);
        int focused = GAME(DlgHasFocusFn, RVA_DLG_HAS_FOCUS)(mgr, dialog, &name);
        if (focused) {
            void *saved = GAME(KeysGetFn, RVA_KEYS_GET)(g_base + RVA_KEYS, PAUSE_SLOT);
            GAME(KeysSetPairFn, RVA_KEYS_SET_PAIR)(g_base + RVA_KEYS_WORK, PAUSE_SLOT, saved);
            GAME(DlgSetFocusFn, RVA_DLG_SET_FOCUS)(mgr, dialog, &name, 0);
        }
        free_game_string(&name, 1, 16);
        if (focused)
            GAME(VoidFn, RVA_TAB4_REFRESH)();
        return focused;
    }
    return 0;
}

static void tab4_event_hook(void *event)
{
    if (!handle_pause_control(event))
        GAME(EventFn, RVA_TAB4_EVENT)(event);
}

/* The dialog layout is game data (GUIDialogs.rc in Patch2.meg). Write a loose copy with a
 * Pause/Play row added to the Interface tab; the game reads loose files over archives.
 * A loose copy we did not write (another mod's) is left alone. */
static char *read_meg_file(const char *meg_path, const char *want, size_t *out_size)
{
    FILE *f = fopen(meg_path, "rb");
    if (!f) return NULL;
    char *result = NULL;
    uint32_t counts[2];
    if (fread(counts, 4, 2, f) != 2) goto done;
    int found = -1;
    for (uint32_t i = 0; i < counts[0]; i++) {
        uint16_t n;
        char name[260];
        if (fread(&n, 2, 1, f) != 1 || n >= sizeof name || fread(name, 1, n, f) != n) goto done;
        name[n] = '\0';
        if (lstrcmpiA(name, want) == 0) found = (int)i;
    }
    if (found < 0) goto done;
    for (uint32_t i = 0; i < counts[1]; i++) {
        uint32_t rec[5]; /* crc, index, size, start, name index */
        if (fread(rec, 4, 5, f) != 5) goto done;
        if ((int)rec[4] != found) continue;
        result = malloc(rec[2] + 1);
        if (!result || _fseeki64(f, rec[3], SEEK_SET) != 0 || fread(result, 1, rec[2], f) != rec[2]) {
            free(result);
            result = NULL;
            goto done;
        }
        result[rec[2]] = '\0';
        *out_size = rec[2];
        break;
    }
done:
    fclose(f);
    return result;
}

static void write_dialog_layout(const char *game_dir)
{
    char path[MAX_PATH + 64];
    size_t size = 0;
    snprintf(path, sizeof path, "%sData\\Patch2.meg", game_dir);
    char *rc = read_meg_file(path, "DATA\\RESOURCES\\GUIDIALOG\\GUIDIALOGS.RC", &size);
    if (!rc) {
        log_line("GUIDialogs.rc not found in Patch2.meg; Pause/Play key not shown in options.");
        return;
    }
    char *dialog = strstr(rc, "IDD_KEYBOARD_OPTIONS_DIALOG_4 DIALOGEX");
    char *end = dialog ? strstr(dialog, "\nEND") : NULL;
    if (!end || strstr(rc, PAUSE_CONTROL)) {
        log_line("Unexpected GUIDialogs.rc layout; Pause/Play key not shown in options.");
        free(rc);
        return;
    }
    static const char row[] =
        "    LTEXT           \"" PAUSE_LABEL "\",IDC_STATIC,460,290,235,23\r\n"
        "    EDITTEXT        " PAUSE_CONTROL ",729,285,108,15,ES_CENTER | \r\n"
        "                    ES_AUTOHSCROLL\r\n";
    size_t head = (size_t)(end + 1 - rc);
    size_t new_size = size + sizeof row - 1;
    char *out = malloc(new_size);
    if (!out) { free(rc); return; }
    memcpy(out, rc, head);
    memcpy(out + head, row, sizeof row - 1);
    memcpy(out + head + sizeof row - 1, rc + head, size - head);
    free(rc);

    snprintf(path, sizeof path, "%sData\\Resources", game_dir);
    CreateDirectoryA(path, NULL);
    snprintf(path, sizeof path, "%sData\\Resources\\GUIDialog", game_dir);
    CreateDirectoryA(path, NULL);
    snprintf(path, sizeof path, "%sData\\Resources\\GUIDialog\\GUIDialogs.rc", game_dir);

    size_t old_size = 0;
    char *old = NULL;
    FILE *f = fopen(path, "rb");
    if (f) {
        _fseeki64(f, 0, SEEK_END);
        old_size = (size_t)_ftelli64(f);
        _fseeki64(f, 0, SEEK_SET);
        old = malloc(old_size + 1);
        if (old && fread(old, 1, old_size, f) == old_size) old[old_size] = '\0';
        else { free(old); old = NULL; }
        fclose(f);
    }
    if (old && !strstr(old, PAUSE_CONTROL)) {
        log_line("Another mod's GUIDialogs.rc is installed; Pause/Play key not shown in options.");
    } else if (!old || old_size != new_size || memcmp(old, out, new_size) != 0) {
        f = fopen(path, "wb");
        if (!f || fwrite(out, 1, new_size, f) != new_size)
            log_line("Could not write %s; Pause/Play key not shown in options.", path);
        if (f) fclose(f);
    }
    free(old);
    free(out);
}

/* ---- Orders while paused ------------------------------------------------------------ */

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

static int is_supported_build(void)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)g_base;
    const IMAGE_NT_HEADERS64 *nt = (const IMAGE_NT_HEADERS64 *)(g_base + dos->e_lfanew);
    return nt->FileHeader.TimeDateStamp == BUILD_TIMESTAMP && nt->OptionalHeader.SizeOfImage == BUILD_IMAGE_SIZE;
}

static void write_code(void *dst, const void *src, size_t len)
{
    DWORD old;
    VirtualProtect(dst, len, PAGE_EXECUTE_READWRITE, &old);
    memcpy(dst, src, len);
    VirtualProtect(dst, len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), dst, len);
}

/* jmp [rip+0]; dq target */
static void write_abs_jump(uint8_t *at, const void *target)
{
    static const uint8_t op[6] = { 0xFF, 0x25, 0, 0, 0, 0 };
    uint64_t t = (uint64_t)target;
    memcpy(at, op, 6);
    memcpy(at + 6, &t, 8);
}

/* Redirects a function start to hook; returns a callable original (stolen bytes + jump back). */
static void *detour(const struct detour_site *d, void *hook)
{
    uint8_t *target = g_base + d->rva;
    uint8_t *tramp = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return NULL;
    memcpy(tramp, target, d->len);
    write_abs_jump(tramp + d->len, target + d->len);

    uint8_t patch[16] = { 0x48, 0xB8 }; /* mov rax, imm64; jmp rax; nop... */
    uint64_t h = (uint64_t)hook;
    memcpy(patch + 2, &h, 8);
    patch[10] = 0xFF;
    patch[11] = 0xE0;
    memset(patch + 12, 0x90, sizeof patch - 12);
    write_code(target, patch, d->len);
    return tramp;
}

/* Executable memory within rel32 reach of the game image. */
static uint8_t *alloc_near_image(size_t size)
{
    uintptr_t base = (uintptr_t)g_base;
    for (uintptr_t a = (base + BUILD_IMAGE_SIZE + 0xFFFF) & ~(uintptr_t)0xFFFF; a < base + 0x70000000; a += 0x10000) {
        void *p = VirtualAlloc((void *)a, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (p) return p;
    }
    for (uintptr_t a = (base - 0x10000) & ~(uintptr_t)0xFFFF; a > base - 0x70000000 && a > 0x10000; a -= 0x10000) {
        void *p = VirtualAlloc((void *)a, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (p) return p;
    }
    return NULL;
}

static int32_t rel32(const uint8_t *next_instruction, const void *target)
{
    return (int32_t)((intptr_t)target - (intptr_t)next_instruction);
}

static int sites_match(void)
{
    const struct detour_site *detours[] = { &D_DISPATCH, &D_PAUSE, &D_DEFAULTS, &D_TAB4_REFRESH };
    for (size_t i = 0; i < sizeof detours / sizeof detours[0]; i++)
        if (memcmp(g_base + detours[i]->rva, detours[i]->bytes, detours[i]->len) != 0) return 0;
    for (size_t i = 0; i < sizeof g_slot_count_patches / sizeof g_slot_count_patches[0]; i++)
        if (memcmp(g_base + g_slot_count_patches[i].rva, g_slot_count_patches[i].old, g_slot_count_patches[i].len) != 0)
            return 0;
    for (size_t i = 0; i < sizeof g_order_sites / sizeof g_order_sites[0]; i++)
        if (memcmp(g_base + g_order_sites[i].rva, g_order_sites[i].bytes, g_order_sites[i].len) != 0) return 0;
    /* lea r8,[rip+X] -> the Interface tab's event handler; lea rax,[rip+X] -> label table */
    uint8_t *lea = g_base + RVA_TAB4_EVENT_LEA;
    if (memcmp(lea, "\x4C\x8D\x05", 3) != 0 || lea + 7 + *(int32_t *)(lea + 3) != g_base + RVA_TAB4_EVENT) return 0;
    for (size_t i = 0; i < sizeof g_label_leas / sizeof g_label_leas[0]; i++) {
        lea = g_base + g_label_leas[i].lea;
        if (memcmp(lea, "\x48\x8D\x05", 3) != 0 || lea + 7 + *(int32_t *)(lea + 3) != g_base + g_label_leas[i].table)
            return 0;
        if (memcmp(g_base + g_label_leas[i].table, g_base + RVA_LABEL_TABLE, 123 * 64) != 0) /* 123 named slots, same in every copy */
            return 0;
    }
    return 1;
}

static void install_pause_key(void)
{
    for (size_t i = 0; i < sizeof g_slot_count_patches / sizeof g_slot_count_patches[0]; i++)
        write_code(g_base + g_slot_count_patches[i].rva, g_slot_count_patches[i].new, g_slot_count_patches[i].len);

    /* Duplicate-key warnings name the other command from the label table. */
    uint8_t *labels = g_near + 0x100;
    memcpy(labels, g_base + RVA_LABEL_TABLE, PAUSE_SLOT * 64);
    memcpy(labels + PAUSE_SLOT * 64, PAUSE_LABEL, sizeof PAUSE_LABEL);
    for (size_t i = 0; i < sizeof g_label_leas / sizeof g_label_leas[0]; i++) {
        uint8_t *lea = g_base + g_label_leas[i].lea;
        int32_t disp = rel32(lea + 7, labels);
        write_code(lea + 3, &disp, 4);
    }

    write_abs_jump(g_near + 0x20, (void *)tab4_event_hook);
    uint8_t *lea = g_base + RVA_TAB4_EVENT_LEA;
    int32_t disp = rel32(lea + 7, g_near + 0x20);
    write_code(lea + 3, &disp, 4);

    g_original_defaults = detour(&D_DEFAULTS, (void *)defaults_hook);
    g_original_tab4_refresh = detour(&D_TAB4_REFRESH, (void *)tab4_refresh_hook);
    g_original_dispatch = detour(&D_DISPATCH, (void *)hotkey_dispatch_hook);
    g_original_pause = detour(&D_PAUSE, (void *)pause_hook);
}

static void install_order_patches(void)
{
    g_pause_count = GAME(int *, RVA_PAUSE_COUNT);
    g_speed_state = (int *)(g_base + RVA_COMMAND_BAR + CB_OFS_SPEED_STATE);
    write_abs_jump(g_near, (void *)order_check_stub);

    for (size_t i = 0; i < sizeof g_order_sites / sizeof g_order_sites[0]; i++) {
        const struct order_check_site *s = &g_order_sites[i];
        uint8_t *site = g_base + s->rva;
        uint8_t patch[7];
        int32_t rel = rel32(site + 5, g_near);
        patch[0] = 0xE8; /* call rel32 */
        memcpy(patch + 1, &rel, 4);
        memset(patch + 5, 0x90, s->len - 5);
        write_code(site, patch, s->len);
    }
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
    int allow_orders = GetPrivateProfileIntA("SpaceBarPause", "AllowOrdersWhilePaused", 1, ini);

    g_base = (uint8_t *)GetModuleHandleA(NULL);
    if (!is_supported_build() || !sites_match()) {
        log_line("Unsupported StarWarsG.exe build; Space Bar Pause disabled, game left unmodified.");
        return;
    }
    g_near = alloc_near_image(0x3000);
    if (!g_near) {
        log_line("No memory near the game image; Space Bar Pause disabled.");
        return;
    }

    write_dialog_layout(dir);
    install_pause_key();
    log_line("Space Bar Pause v" MOD_VERSION " active: Pause/Play key in Options > Keyboard > Interface.");
    if (allow_orders) {
        install_order_patches();
        log_line("Orders while paused enabled (%u checks relaxed).",
                 (unsigned)(sizeof g_order_sites / sizeof g_order_sites[0]));
    }
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
