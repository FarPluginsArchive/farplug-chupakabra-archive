#ifndef _UTIL_H
#define _UTIL_H

#ifdef DEBUG
#define LOG_MSG(msg) OutputDebugStringA((msg + '\n').data());
#define LOG_UMSG(msg) OutputDebugStringW((msg + '\n').data());
#define LOG_ERR(e) OutputDebugStringW(UnicodeString::format(L"File: %s Line: %u \"%s\"\n", oem_to_unicode(e.file).data(), e.line, e.message().data()).data());
#else // DEBUG
#define LOG_MSG(msg)
#define LOG_UMSG(msg)
#define LOG_ERR(e) (e)
#endif // DEBUG

void unicode_to_oem(AnsiString& oem_str, const UnicodeString& u_str);
void oem_to_unicode(UnicodeString& u_str, const AnsiString& oem_str);
AnsiString unicode_to_oem(const UnicodeString& u_str);
UnicodeString oem_to_unicode(const AnsiString& oem_str);
void unicode_to_ansi(AnsiString& a_str, const UnicodeString& u_str);
void ansi_to_unicode(UnicodeString& u_str, const AnsiString& a_str);
AnsiString unicode_to_ansi(const UnicodeString& u_str);
UnicodeString ansi_to_unicode(const AnsiString& a_str);

UnicodeString word_wrap(const UnicodeString& message, unsigned wrap_bound);
UnicodeString format_data_size(unsigned __int64 size, const ObjectArray<UnicodeString>& suffixes);
UnicodeString format_time(unsigned __int64 t /* ms */);
UnicodeString fit_str(const UnicodeString& path, unsigned size);
UnicodeString center(const UnicodeString& str, unsigned width);
unsigned __int64 mul_div(unsigned __int64 a, unsigned __int64 b, unsigned __int64 c);

#ifdef _M_X64
#  define PLUGIN_TYPE L" uni x64"
#else
#  define PLUGIN_TYPE L" uni"
#endif

struct ModuleVersion {
  unsigned major;
  unsigned minor;
  unsigned patch;
  unsigned revision;
};
ModuleVersion get_module_version(HINSTANCE module);
const UnicodeString extract_file_name(const UnicodeString& file_path);
void unquote(UnicodeString& str);

UnicodeString make_temp_file();
UnicodeString format_file_time(const FILETIME& file_time);

UnicodeString add_trailing_slash(const UnicodeString& file_path);
UnicodeString del_trailing_slash(const UnicodeString& file_path);

intptr_t far_control_int(HANDLE h_panel, FILE_CONTROL_COMMANDS command, intptr_t param);
intptr_t far_control_ptr(HANDLE h_panel, FILE_CONTROL_COMMANDS command, const void* param);
PluginPanelItem* far_get_panel_item(HANDLE h_panel, intptr_t index, const PanelInfo& pi);
PluginPanelItem* far_get_selected_panel_item(HANDLE h_panel, intptr_t index, const PanelInfo& pi);
UnicodeString far_get_current_dir();
UnicodeString far_get_full_path(const UnicodeString& file_path);
void far_set_progress_state(TBPFLAG state);
void far_set_progress_value(unsigned __int64 completed, unsigned __int64 total);

#endif // _UTIL_H
