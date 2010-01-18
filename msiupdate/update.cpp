#include "msg.h"

#include "utils.hpp"
#include "farutils.hpp"
#include "sysutils.hpp"
#include "iniparse.hpp"
#include "utils.hpp"
#include "options.hpp"
#include "ui.hpp"
#include "inet.hpp"
#include "transact.hpp"
#include "update.hpp"

namespace Update {

const wchar_t* c_param_last_check_time = L"last_check_time";
const wchar_t* c_param_last_check_version = L"last_check_version";
const wchar_t* c_param_update_version = L"update_version";
#ifdef _M_IX86
const char* c_upgrade_code = "{67A59013-488F-4388-81F3-828A1DCEDA32}";
const char* c_platform = "x86";
const wchar_t* c_update_script = L"update2.php?p=32";
#endif
#ifdef _M_X64
const char* c_upgrade_code = "{83140F3F-1457-42EB-8053-233293664714}";
const char* c_platform = "x64";
const wchar_t* c_update_script = L"update2.php?p=64";
#endif
const wchar_t* changelog_url = L"http://farmanager.com/svn/trunk/unicode_far/changelog";
const unsigned c_exit_wait = 6;
const unsigned c_update_period = 12 * 60 * 60;

unsigned current_version;

struct UpdateInfo {
  unsigned version;
  wstring package_name;
};

UpdateInfo parse_update_info(const string& text) {
  Ini::File update_ini;
  update_ini.parse(text);
  string ver_major = update_ini.get("far", "major");
  string ver_minor = update_ini.get("far", "minor");
  string ver_build = update_ini.get("far", "build");
  UpdateInfo update_info;
  update_info.version = MAKE_VERSION(str_to_int(ver_major), str_to_int(ver_minor), str_to_int(ver_build));
  update_info.package_name = widen(update_ini.get("far", "msi"));
  return update_info;
}

void check_product_installed() {
  char product_guid[39];
  if (MsiEnumRelatedProducts(c_upgrade_code, 0, 0, product_guid) != ERROR_SUCCESS)
    FAIL_MSG(Far::get_msg(MSG_ERROR_NO_MSI));
}

wstring get_update_url() {
  wstring update_url = L"http://www.farmanager.com/";
  if (g_options.update_stable_builds)
    update_url += L"files/";
  else
    update_url += L"nightly/";
  return update_url;
}

void show_changelog(unsigned build1, unsigned build2) {
  wstring text = ansi_to_unicode(load_url(changelog_url, g_options.http), 1251);
  const wchar_t* c_expr_prefix = L"[^\\s]+\\s+\\d+\\.\\d+\\.\\d+\\s+\\d+:\\d+:\\d+\\s+[+-]?\\d+\\s+-\\s+build\\s+";
  const wchar_t* c_expr_suffix = L"\\s*";
  Far::Regex regex;
  size_t pos1 = regex.search(c_expr_prefix + int_to_str(build1) + c_expr_suffix, text);
  if (pos1 == -1)
    pos1 = text.size();
  size_t pos2 = regex.search(c_expr_prefix + int_to_str(build2) + c_expr_suffix, text);
  if (pos2 == -1)
    pos2 = 0;
  CHECK(pos1 > pos2);

  TempFile temp_file;
  {
    HANDLE h_file = CreateFileW(temp_file.get_path().c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK_SYS(h_file != INVALID_HANDLE_VALUE);
    CleanHandle h_file_clean(h_file);

    const wchar_t sig = 0xFEFF;
    DWORD size_written;
    CHECK_SYS(WriteFile(h_file, &sig, sizeof(sig), &size_written, NULL));
    CHECK_SYS(WriteFile(h_file, text.data() + pos2, static_cast<DWORD>((pos1 - pos2) * sizeof(wchar_t)), &size_written, NULL));
  }

  Far::viewer(temp_file.get_path(), L"Changelog");
  Far::flush_screen();
}

enum UpdateDialogResult {
  udrYes,
  udrNo,
  udrCancel,
};

class UpdateDialog: public Far::Dialog {
private:
  enum {
    c_client_xs = 40
  };

  const UpdateInfo& update_info;

  int changelog_ctrl_id;
  int yes_ctrl_id;
  int no_ctrl_id;
  int cancel_ctrl_id;

  LONG_PTR dialog_proc(int msg, int param1, LONG_PTR param2) {
    if (msg == DN_INITDIALOG) {
      set_focus(yes_ctrl_id);
      return TRUE;
    }
    else if ((msg == DN_BTNCLICK) && (param1 == changelog_ctrl_id)) {
      show_changelog(VER_BUILD(current_version), VER_BUILD(update_info.version));
    }
    return default_dialog_proc(msg, param1, param2);
  }

public:
  UpdateDialog(const UpdateInfo& update_info): Far::Dialog(Far::get_msg(MSG_PLUGIN_NAME), c_client_xs, L"update"), update_info(update_info) {
  }

  UpdateDialogResult show() {
    wostringstream st;
    st << Far::get_msg(MSG_UPDATE_NEW_VERSION) << L' ' << VER_MAJOR(update_info.version) << L'.' << VER_MINOR(update_info.version) << L'.' << VER_BUILD(update_info.version);
    label(st.str());
    new_line();
    label(Far::get_msg(MSG_UPDATE_QUESTION));
    new_line();
    changelog_ctrl_id = button(Far::get_msg(MSG_UPDATE_CHANGELOG), DIF_BTNNOCLOSE);
    new_line();
    separator();
    new_line();
    yes_ctrl_id = def_button(Far::get_msg(MSG_BUTTON_YES), DIF_CENTERGROUP);
    no_ctrl_id = button(Far::get_msg(MSG_BUTTON_NO), DIF_CENTERGROUP);
    cancel_ctrl_id = button(Far::get_msg(MSG_BUTTON_CANCEL), DIF_CENTERGROUP);
    new_line();

    int item = Far::Dialog::show();
    if (item == yes_ctrl_id) return udrYes;
    else if (item == no_ctrl_id) return udrNo;
    else return udrCancel;
  }
};

void prepare_directory(const wstring& dir) {
  if (GetFileAttributesW(dir.c_str()) == INVALID_FILE_ATTRIBUTES) {
    if (!is_root_path(dir)) {
      prepare_directory(extract_file_path(dir));
      CHECK_SYS(CreateDirectoryW(dir.c_str(), NULL));
    }
  }
}

void save_to_cache(const string& package, const wstring& cache_dir, const wstring& package_name) {
  prepare_directory(cache_dir);
  ktm::Transaction transaction;
  {
    const wchar_t* c_param_cache_index = L"cache_index";
    ktm::Key plugin_key(HKEY_CURRENT_USER, (add_trailing_slash(Far::get_root_key_name()) + c_plugin_key_name).c_str(), KEY_QUERY_VALUE | KEY_SET_VALUE, transaction.handle());

    list<wstring> cache_index = split(reg_query_value(plugin_key.handle(), c_param_cache_index), L'\n');

    while (cache_index.size() && cache_index.size() >= g_options.cache_max_size) {
      ktm::DeleteFile((add_trailing_slash(cache_dir) + cache_index.front()).c_str(), transaction.handle());
      cache_index.erase(cache_index.begin());
    }

    ktm::File package_file((add_trailing_slash(cache_dir) + package_name).c_str(), GENERIC_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, transaction.handle());
    DWORD size_written;
    CHECK_SYS(WriteFile(package_file.handle(), package.data(), static_cast<DWORD>(package.size()), &size_written, NULL));

    cache_index.push_back(package_name);
    reg_set_value(plugin_key.handle(), c_param_cache_index, combine(cache_index, L'\n'));
  }
  transaction.commit();
}

void execute() {
  check_product_installed();
  string update_url_text = load_url(get_update_url() + c_update_script, g_options.http);
  UpdateInfo update_info = parse_update_info(update_url_text);
  Options::set_int(c_param_update_version, update_info.version);
  if (update_info.version <= current_version) {
    info_dlg(Far::get_msg(MSG_UPDATE_NO_NEW_VERSION));
    return;
  }
  UpdateDialogResult res = UpdateDialog(update_info).show();
  if (res == udrYes) {
    WindowInfo window_info;
    bool editor_unsaved = false;
    for (unsigned idx = 0; !editor_unsaved && Far::get_short_window_info(idx, window_info); idx++) {
      if ((window_info.Type == WTYPE_EDITOR) && window_info.Modified)
        editor_unsaved = true;
    }
    if (editor_unsaved)
      FAIL_MSG(Far::get_msg(MSG_ERROR_EDITOR_UNSAVED));

    wstring cache_dir;
    if (g_options.cache_enabled) {
      cache_dir = expand_env_vars(g_options.cache_dir);
      string package = load_url(get_update_url() + update_info.package_name, g_options.http);
      save_to_cache(package, cache_dir, update_info.package_name);
    }

    wostringstream st;
    st << L"msiexec /promptrestart ";
    if (!g_options.use_full_install_ui) {
      st << L"/qr ";
    }
    if (g_options.logged_install) {
      wchar_t temp_path[MAX_PATH];
      CHECK_SYS(GetTempPathW(ARRAYSIZE(temp_path), temp_path));
      st << L"/log \"" << add_trailing_slash(temp_path) << L"MsiUpdate_" << VER_MAJOR(current_version) << L"_" << widen(c_platform) << L".log\" ";
    }
    if (g_options.cache_enabled)
      st << "/i \"" << add_trailing_slash(cache_dir) + update_info.package_name + L"\"";
    else
      st << "/i \"" << get_update_url() + update_info.package_name + L"\"";
    if (!g_options.install_properties.empty())
      st << L" " << g_options.install_properties;
    wstring command = st.str();

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    CHECK_SYS(CreateProcessW(NULL, const_cast<LPWSTR>(command.c_str()), NULL, NULL, FALSE, CREATE_DEFAULT_ERROR_MODE, NULL, NULL, &si, &pi));
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    vector<DWORD> keys;
    keys.push_back(KEY_F10);
    if (Far::get_confirmation_settings() & FCS_EXIT)
      keys.push_back(KEY_ENTER);
    Far::post_keys(keys);
  }
  else if (res == udrNo) {
    Options::set_int(c_param_last_check_version, update_info.version);
  }
}

HANDLE g_h_abort = NULL;
HANDLE g_h_thread = NULL;
wstring g_update_url;
HttpOptions g_http_options;
unsigned __stdcall check_thread(void* arg) {
  try {
    string update_url_text = load_url(g_update_url, g_http_options, g_h_abort);
    UpdateInfo update_info = parse_update_info(update_url_text);
    Options::set_int(c_param_update_version, update_info.version);
    Far::call_user_apc(reinterpret_cast<void*>(cmdClean));
  }
  catch (...) {
  }
  return 0;
}

void init() {
  current_version = Far::get_version();
  check_product_installed();
  unsigned update_version = Options::get_int(c_param_update_version);
  unsigned last_check_version = Options::get_int(c_param_last_check_version);
  if ((update_version > current_version) && (update_version > last_check_version)) {
    Far::call_user_apc(reinterpret_cast<void*>(cmdExecute));
    return;
  }
  time_t curr_time = time(NULL);
  CHECK(curr_time != -1);
  curr_time /= c_update_period;
  unsigned last_check_time = Options::get_int(c_param_last_check_time);
  if (curr_time == last_check_time)
    return;
  Options::set_int(c_param_last_check_time, static_cast<unsigned>(curr_time));
  g_update_url = get_update_url() + c_update_script;
  g_http_options = g_options.http;
  g_h_abort = CreateEvent(NULL, TRUE, FALSE, NULL);
  CHECK_SYS(g_h_abort);
  unsigned th_id;
  g_h_thread = reinterpret_cast<HANDLE>(_beginthreadex(NULL, 0, check_thread, NULL, 0, &th_id));
  CHECK_SYS(g_h_thread);
}

void clean() {
  if (g_h_thread) {
    SetEvent(g_h_abort);
    DWORD wait_res = WaitForSingleObject(g_h_thread, c_exit_wait * 1000);
    if (wait_res != WAIT_OBJECT_0) {
      TerminateThread(g_h_thread, 0);
    }
    CloseHandle(g_h_thread);
    g_h_thread = NULL;
  }
  if (g_h_abort) {
    CloseHandle(g_h_abort);
    g_h_abort = NULL;
  }
}

}