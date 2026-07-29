/* GENERATED from lang.json's "en" object — do not edit by hand.
 *
 * Compiled-in English, used when lang.json cannot be read. That happens
 * for real: the self-updater replaces only the binary, so anyone updating
 * in-app from a build that predates lang.json has no such file on the card,
 * and without this the whole UI would render raw keys ("settings_title").
 * Regenerate whenever the catalogue gains or renames a key.
 */
#include "i18n_fallback.h"
#include <string.h>

static const struct { const char *key; const char *text; } EN[] = {
    { "app_title", "Rom Downloader By AEY" },
    { "splash", "Rom Downloader\nBy AEY" },
    { "hint_emu_select", "Up/Down: Move   A: Select   Sel: Settings   Start: Quit" },
    { "hint_rom_list", "A:DL  Y:Search  X:Fav  Sel:Filter  B:Back" },
    { "hint_confirm", "A: Confirm   B: Cancel" },
    { "hint_filters", "Up/Down: Row  Left/Right: Change  A: Apply  B: Cancel" },
    { "hint_update", "A: Download & Install   B: Ignore" },
    { "hint_settings", "Up/Down: Move   A: Change   B: Back" },
    { "loading_fmt", "Loading %s..." },
    { "checking_updates", "Checking for updates..." },
    { "signing_in_startup", "Signing in to archive.org..." },
    { "suffix_account_required", "%s (account required)" },
    { "suffix_source_unavailable", "%s (source unavailable)" },
    { "empty_no_matching", "No matching roms" },
    { "empty_no_roms", "No roms" },
    { "empty_source_unavailable", "Source unavailable right now" },
    { "empty_needs_account", "Needs an archive.org account.\nPress A to sign in." },
    { "search_placeholder", "Search roms..." },
    { "confirm_download", "Download this rom?\n\n%s\n%s" },
    { "downloaded", "Downloaded" },
    { "downloading", "Downloading..." },
    { "downloading_queued_fmt", "Downloading... (+%d)" },
    { "download_failed", "Download failed" },
    { "filters_title", "Filters" },
    { "filter_region", "Region" },
    { "filter_favourites_only", "Favourites only" },
    { "on", "On" },
    { "off", "Off" },
    { "region_all", "All" },
    { "region_usa", "USA" },
    { "region_europe", "Europe" },
    { "region_japan", "Japan" },
    { "region_world", "World" },
    { "update_available", "Update Available" },
    { "update_version_ready_fmt", "Version %s is ready" },
    { "update_downloading", "Downloading update..." },
    { "update_failed_install", "Update failed (couldn't install)" },
    { "update_failed_download", "Update download failed" },
    { "login_title", "Sign in to archive.org" },
    { "login_email", "email" },
    { "login_password", "password" },
    { "login_hint_email", "Enter your email, then press the check key. B cancels." },
    { "login_hint_password", "Now enter your password, then press the check again" },
    { "login_hint_prefilled", "Loaded from .env. Check key confirms, X clears a field." },
    { "login_in_progress", "Signing in..." },
    { "login_ok", "Signed in" },
    { "login_fail", "Sign-in failed" },
    { "login_fail_reason_fmt", "Sign-in failed: %s" },
    { "login_fail_2fa_hint", "If your account uses 2-step verification, see archive_cookies.txt.example on the card." },
    { "login_need_fields", "Enter email and password" },
    { "login_no_response", "No response - check Wi-Fi" },
    { "login_no_cacert", "Missing cacert.pem - cannot sign in safely" },
    { "login_no_session", "Signed in, but no session returned" },
    { "login_bad_reply", "Unexpected reply from archive.org" },
    { "login_no_prepare", "Could not prepare sign-in request" },
    { "login_no_save", "Could not save session to card" },
    { "settings_title", "Settings" },
    { "settings_language", "Language" },
    { "settings_sound", "Navigation sounds" },
    { "settings_account", "archive.org" },
    { "settings_signed_in_as_fmt", "Signed in as %s" },
    { "settings_signed_out", "Not signed in" },
    { "settings_sign_in", "Sign in" },
    { "settings_sign_out", "Sign out" },
    { "settings_cleanup", "Clear unfinished downloads" },
    { "settings_cleanup_none", "Nothing to clear" },
    { "settings_cleanup_done_fmt", "Cleared %d file(s), %s freed" },
    { "settings_signed_out_done", "Signed out" },
    { "login_tls_failed", "Secure connection failed - check the device date/time" },
    { "login_timeout", "Timed out - try again" },
    { "login_failed_code_fmt", "Sign-in failed (error %d) - see log.txt" },
    { "settings_sources", "Rom sources" },
    { "sources_title", "Rom sources" },
    { "hint_sources", "A: Edit   X: On/Off   Y: Defaults   B: Back" },
    { "sources_edit_hint", "An archive.org item name, or a full URL to a listing you host." },
    { "sources_saved", "Source saved" },
    { "sources_reset_done", "Defaults restored" },
};

const char *i18n_fallback(const char *key) {
    for (unsigned i = 0; i < sizeof(EN) / sizeof(EN[0]); i++) {
        if (strcmp(EN[i].key, key) == 0) return EN[i].text;
    }
    return NULL;
}
