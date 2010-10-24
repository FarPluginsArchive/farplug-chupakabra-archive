#include "msg.h"

#include "utils.hpp"
#include "sysutils.hpp"
#include "farutils.hpp"
#include "common.hpp"
#include "ui.hpp"
#include "archive.hpp"
#include "options.hpp"

wstring g_plugin_prefix;

class Plugin {
private:
  Archive archive;

  wstring current_dir;
  wstring extract_dir;
  wstring host_file;
  wstring panel_title;
  vector<InfoPanelLine> info_lines;

public:
  Plugin() {
  }

  Plugin(const wstring& file_path, bool auto_detect) {
    Archive::max_check_size = g_options.max_check_size;
    vector<Archive> archives = Archive::detect(file_path, !auto_detect);

    if (archives.size() == 0)
      FAIL(E_ABORT);

    int format_idx;
    if (archives.size() == 1) {
      format_idx = 0;
    }
    else {
      vector<wstring> format_names;
      for (unsigned i = 0; i < archives.size(); i++) {
        format_names.push_back(archives[i].arc_chain.to_string());
      }
      format_idx = Far::menu(Far::get_msg(MSG_PLUGIN_NAME), format_names);
      if (format_idx == -1)
        FAIL(E_ABORT);
    }

    archive = archives[format_idx];
  }

  void info(OpenPluginInfo* opi) {
    opi->StructSize = sizeof(OpenPluginInfo);
    opi->Flags = OPIF_USEFILTER | OPIF_USESORTGROUPS | OPIF_USEHIGHLIGHTING | OPIF_ADDDOTS;
    opi->CurDir = current_dir.c_str();
    panel_title = Far::get_msg(MSG_PLUGIN_NAME);
    if (archive.is_open()) {
      panel_title += L":" + archive.arc_chain.to_string() + L":" + extract_file_name(archive.arc_path);
      host_file = extract_file_name(archive.arc_path);
    }
    opi->HostFile = host_file.c_str();
    opi->Format = g_plugin_prefix.c_str();
    opi->PanelTitle = panel_title.c_str();
    opi->StartPanelMode = '0' + g_options.panel_view_mode;
    opi->StartSortMode = g_options.panel_sort_mode;
    opi->StartSortOrder = g_options.panel_reverse_sort;

    info_lines.clear();
    info_lines.reserve(archive.arc_attr.size() + 1);
    InfoPanelLine ipl;
    ipl.Text = panel_title.c_str();
    ipl.Data = nullptr;
    ipl.Separator = 1;
    info_lines.push_back(ipl);
    for_each(archive.arc_attr.begin(), archive.arc_attr.end(), [&] (const Attr& attr) {
      ipl.Text = attr.name.c_str();
      ipl.Data = attr.value.c_str();
      ipl.Separator = 0;
      info_lines.push_back(ipl);
    });
    opi->InfoLines = info_lines.data();
    opi->InfoLinesNumber = static_cast<int>(info_lines.size());
  }

  void set_dir(const wstring& dir) {
    if (!archive.is_open())
      FAIL(E_ABORT);
    wstring new_dir;
    if (dir == L"\\")
      new_dir.assign(dir);
    else if (dir == L"..")
      new_dir = extract_file_path(current_dir);
    else if (dir[0] == L'\\') // absolute path
      new_dir.assign(dir);
    else
      new_dir.assign(L"\\").append(add_trailing_slash(remove_path_root(current_dir))).append(dir);

    if (new_dir == L"\\")
      new_dir.clear();

    archive.find_dir(new_dir);
    current_dir = new_dir;
  }

  void list(PluginPanelItem** panel_items, int* items_number) {
    if (!archive.is_open())
      FAIL(E_ABORT);
    UInt32 dir_index = archive.find_dir(current_dir);
    FileIndexRange dir_list = archive.get_dir_list(dir_index);
    size_t size = dir_list.second - dir_list.first;
    PluginPanelItem* items = new PluginPanelItem[size];
    memset(items, 0, size * sizeof(PluginPanelItem));
    try {
      unsigned idx = 0;
      for_each(dir_list.first, dir_list.second, [&] (UInt32 file_index) {
        const ArcFileInfo& file_info = archive.get_file_info(file_index);
        FAR_FIND_DATA& fdata = items[idx].FindData;
        const DWORD c_valid_attributes = FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM;
        fdata.dwFileAttributes = file_info.attr & c_valid_attributes;
        fdata.ftCreationTime = file_info.ctime;
        fdata.ftLastAccessTime = file_info.atime;
        fdata.ftLastWriteTime = file_info.mtime;
        fdata.nFileSize = file_info.size;
        fdata.nPackSize = file_info.psize;
        fdata.lpwszFileName = file_info.name.c_str();
        items[idx].UserData = file_index;
        idx++;
      });
    }
    catch (...) {
      delete[] items;
      throw;
    }
    *panel_items = items;
    *items_number = static_cast<int>(size);
  }

  static wstring get_separate_dir_path(const wstring& dst_dir, const wstring& arc_path) {
    wstring final_dir = add_trailing_slash(dst_dir) + extract_file_name(arc_path);
    wstring ext = extract_file_ext(final_dir);
    final_dir.erase(final_dir.size() - ext.size(), ext.size());
    if (GetFileAttributesW(long_path(final_dir).c_str()) != INVALID_FILE_ATTRIBUTES) {
      unsigned n = 0;
      while (GetFileAttributesW(long_path(final_dir + L"." + int_to_str(n)).c_str()) != INVALID_FILE_ATTRIBUTES && n < 100) n++;
      final_dir += L"." + int_to_str(n);
    }
    return final_dir;
  }

  void get_files(const PluginPanelItem* panel_items, int items_number, int move, const wchar_t** dest_path, int op_mode) {
    if (items_number == 1 && wcscmp(panel_items[0].FindData.lpwszFileName, L"..") == 0) return;
    ExtractOptions options;
    options.dst_dir = *dest_path;
    options.move_enabled = archive.updatable();
    options.move_files = move != 0 && options.move_enabled;
    options.delete_archive = false;
    options.show_dialog = (op_mode & (OPM_FIND | OPM_VIEW | OPM_EDIT | OPM_QUICKVIEW)) == 0;
    if ((op_mode & OPM_SILENT) && (op_mode & OPM_TOPLEVEL) == 0)
      options.show_dialog = true;
    if (op_mode & (OPM_FIND | OPM_QUICKVIEW))
      options.ignore_errors = true;
    else
      options.ignore_errors = g_options.extract_ignore_errors;
    if (options.show_dialog) {
      options.overwrite = static_cast<OverwriteOption>(g_options.extract_overwrite);
      options.separate_dir = static_cast<TriState>(g_options.extract_separate_dir);
    }
    else {
      options.overwrite = ooOverwrite;
      options.separate_dir = triFalse;
    }
    if (options.show_dialog) {
      if (!extract_dialog(options))
        FAIL(E_ABORT);
      if (options.dst_dir.empty())
        options.dst_dir = L".";
      if (!is_absolute_path(options.dst_dir))
        options.dst_dir = Far::get_absolute_path(options.dst_dir);
      if (options.dst_dir != *dest_path) {
        extract_dir = options.dst_dir;
        *dest_path = extract_dir.c_str();
      }
      if (options.separate_dir == triTrue || (options.separate_dir == triUndef && items_number > 1 && (op_mode & OPM_TOPLEVEL))) {
        options.dst_dir = get_separate_dir_path(options.dst_dir, archive.arc_path);
      }
      if (!options.password.empty())
        archive.password = options.password;
      g_options.extract_ignore_errors = options.ignore_errors;
      g_options.extract_overwrite = options.overwrite;
      g_options.extract_separate_dir = options.separate_dir;
      g_options.save();
    }

    UInt32 src_dir_index = archive.find_dir(current_dir);

    vector<UInt32> indices;
    indices.reserve(items_number);
    for (int i = 0; i < items_number; i++) {
      indices.push_back(static_cast<UInt32>(panel_items[i].UserData));
    }

    ErrorLog error_log;
    archive.extract(src_dir_index, indices, options, error_log);
    if (!error_log.empty()) {
      if (options.show_dialog)
        show_error_log(error_log);
    }
    else {
      if (options.delete_archive) {
        archive.close();
        archive.delete_archive();
        Far::close_plugin(this, archive.arc_dir());
      }
      else if (options.move_files)
        archive.delete_files(indices);
      Far::progress_notify();
    }
  }

  static void bulk_extract(const vector<wstring> file_list) {
    ExtractOptions options;
    wstring dst_dir;
    options.dst_dir = Far::get_panel_dir(PANEL_PASSIVE);
    options.move_enabled = false;
    options.move_files = false;
    options.delete_archive = false;
    options.show_dialog = true;
    options.ignore_errors = g_options.extract_ignore_errors;
    options.overwrite = static_cast<OverwriteOption>(g_options.extract_overwrite);
    options.separate_dir = static_cast<TriState>(g_options.extract_separate_dir);
    if (options.show_dialog) {
      if (!extract_dialog(options))
        FAIL(E_ABORT);
      dst_dir = options.dst_dir;
      if (dst_dir.empty())
        dst_dir = L".";
      if (!is_absolute_path(dst_dir))
        dst_dir = Far::get_absolute_path(options.dst_dir);

      g_options.extract_ignore_errors = options.ignore_errors;
      g_options.extract_overwrite = options.overwrite;
      g_options.extract_separate_dir = options.separate_dir;
      g_options.save();
    }

    ErrorLog error_log;
    for (unsigned i = 0; i < file_list.size(); i++) {
      vector<Archive> archives = Archive::detect(file_list[i], false);
      if (archives.empty())
        continue;
      Archive& archive = archives[0];

      if (archive.password.empty())
        archive.password = options.password;

      archive.make_index();

      FileIndexRange dir_list = archive.get_dir_list(c_root_index);
      vector<UInt32> indices;
      indices.reserve(dir_list.second - dir_list.first);
      for_each(dir_list.first, dir_list.second, [&] (UInt32 file_index) {
        indices.push_back(file_index);
      });

      if (options.separate_dir == triTrue || (options.separate_dir == triUndef && indices.size() > 1))
        options.dst_dir = get_separate_dir_path(dst_dir, archive.arc_path);
      else
        options.dst_dir = dst_dir;

      size_t error_count = error_log.size();
      archive.extract(c_root_index, indices, options, error_log);

      if (options.delete_archive && error_count == error_log.size()) {
        archive.close();
        archive.delete_archive();
      }
    }

    if (!error_log.empty()) {
      show_error_log(error_log);
    }
    else {
      Far::progress_notify();
    }
  }

  void test_files(struct PluginPanelItem* panel_items, int items_number, int op_mode) {
    UInt32 src_dir_index = archive.find_dir(current_dir);
    vector<UInt32> indices;
    indices.reserve(items_number);
    for (int i = 0; i < items_number; i++) {
      indices.push_back(static_cast<UInt32>(panel_items[i].UserData));
    }
    archive.test(src_dir_index, indices);
    Far::info_dlg(Far::get_msg(MSG_PLUGIN_NAME), Far::get_msg(MSG_TEST_OK));
  }

  void put_files(const PluginPanelItem* panel_items, int items_number, int move, const wchar_t* src_path, int op_mode) {
    if (items_number == 1 && wcscmp(panel_items[0].FindData.lpwszFileName, L"..") == 0)
      return;
    UpdateOptions options;
    bool new_arc = !archive.is_open();
    if (!new_arc && !archive.updatable()) {
      FAIL_MSG(Far::get_msg(MSG_ERROR_NOT_UPDATABLE));
    }
    if (new_arc) {
      if (items_number == 1 || is_root_path(src_path))
        options.arc_path = panel_items[0].FindData.lpwszFileName;
      else
        options.arc_path = extract_file_name(src_path);
      ArcTypes arc_types = ArcAPI::formats().find_by_name(g_options.update_arc_format_name);
      if (arc_types.empty())
        options.arc_type = c_guid_7z;
      else
        options.arc_type = arc_types.front();
      options.sfx_module_idx = g_options.update_sfx_module_idx;
      if (ArcAPI::formats().count(options.arc_type))
        options.arc_path += ArcAPI::formats().at(options.arc_type).default_extension();

      options.level = g_options.update_level;
      options.method = g_options.update_method;
      options.solid = g_options.update_solid;
      options.encrypt = false;
      options.encrypt_header_defined = true;
      options.encrypt_header = g_options.update_encrypt_header;
      options.volume_size = g_options.update_volume_size;
    }
    else {
      options.arc_type = archive.arc_chain.back().type; // required to set update properties
      archive.load_update_props();
      options.level = archive.level;
      options.method = archive.method;
      options.solid = archive.solid;
      options.encrypt = archive.encrypted;
      options.encrypt_header_defined = false;
      options.password = archive.password;
    }
    options.create_sfx = false;
    options.enable_volumes = false;
    options.show_password = g_options.update_show_password;
    options.move_files = move != 0;
    options.open_shared = (Far::adv_control(ACTL_GETSYSTEMSETTINGS) & FSS_COPYFILESOPENEDFORWRITING) != 0;
    options.ignore_errors = g_options.update_ignore_errors;

    if (!update_dialog(new_arc, options))
      FAIL(E_ABORT);
    if (ArcAPI::formats().count(options.arc_type) == 0)
      FAIL_MSG(Far::get_msg(MSG_ERROR_NO_FORMAT));
    if (new_arc) {
      if (!is_absolute_path(options.arc_path))
        options.arc_path = Far::get_absolute_path(options.arc_path);
      if (GetFileAttributesW(long_path(options.arc_path).c_str()) != INVALID_FILE_ATTRIBUTES) {
        if (Far::message(Far::get_msg(MSG_PLUGIN_NAME) + L"\n" + Far::get_msg(MSG_UPDATE_DLG_CONFIRM_OVERWRITE), 0, FMSG_MB_YESNO) != 0)
          FAIL(E_ABORT);
      }
      g_options.update_arc_format_name = ArcAPI::formats().at(options.arc_type).name;
      g_options.update_sfx_module_idx = options.sfx_module_idx;
      g_options.update_volume_size = options.volume_size;
      g_options.update_level = options.level;
      g_options.update_method = options.method;
      g_options.update_solid = options.solid;
      g_options.update_encrypt_header = options.encrypt_header;
    }
    else {
      archive.level = options.level;
      archive.method = options.method;
      archive.solid = options.solid;
      archive.encrypted = options.encrypt;
    }
    g_options.update_show_password = options.show_password;
    g_options.update_ignore_errors = options.ignore_errors;
    g_options.save();

    ErrorLog error_log;
    if (new_arc)
      archive.create(src_path, panel_items, items_number, options, error_log);
    else
      archive.update(src_path, panel_items, items_number, remove_path_root(current_dir), options, error_log);

    if (!error_log.empty()) {
      show_error_log(error_log);
    }
    else {
      Far::progress_notify();
    }

    if (new_arc) {
      if (upcase(Far::get_panel_dir(PANEL_ACTIVE)) == upcase(extract_file_path(options.arc_path)))
        Far::panel_go_to_file(PANEL_ACTIVE, options.arc_path);
      if (upcase(Far::get_panel_dir(PANEL_PASSIVE)) == upcase(extract_file_path(options.arc_path)))
        Far::panel_go_to_file(PANEL_PASSIVE, options.arc_path);
    }
  }

  void delete_files(const PluginPanelItem* panel_items, int items_number, int op_mode) {
    if (items_number == 1 && wcscmp(panel_items[0].FindData.lpwszFileName, L"..") == 0) return;

    bool show_dialog = (op_mode & (OPM_SILENT | OPM_FIND | OPM_VIEW | OPM_EDIT | OPM_QUICKVIEW)) == 0;
    if (show_dialog) {
      if (Far::message(Far::get_msg(MSG_PLUGIN_NAME) + L"\n" + Far::get_msg(MSG_DELETE_DLG_CONFIRM), 0, FMSG_MB_OKCANCEL) != 0)
        FAIL(E_ABORT);
    }

    vector<UInt32> indices;
    indices.reserve(items_number);
    for (int i = 0; i < items_number; i++) {
      indices.push_back(static_cast<UInt32>(panel_items[i].UserData));
    }
    archive.delete_files(indices);

    Far::progress_notify();
  }

  void show_attr() {
    AttrList attr_list;
    Far::PanelItem panel_item = Far::get_current_panel_item(PANEL_ACTIVE);
    if (panel_item.file_name == L"..") {
      if (is_root_path(current_dir)) {
        attr_list = archive.arc_attr;
      }
      else {
        attr_list = archive.get_attr_list(archive.find_dir(current_dir));
      }
    }
    else {
      attr_list = archive.get_attr_list(static_cast<UInt32>(panel_item.user_data));
    }
    if (!attr_list.empty())
      attr_dialog(attr_list);
  }

  void close() {
    PanelInfo panel_info;
    if (Far::get_panel_info(this, panel_info)) {
      g_options.panel_view_mode = panel_info.ViewMode;
      g_options.panel_sort_mode = panel_info.SortMode;
      g_options.panel_reverse_sort = (panel_info.Flags & PFLAGS_REVERSESORTORDER) != 0;
      g_options.save();
    }
  }
};

TriState auto_detect_next_time = triUndef;

int WINAPI GetMinFarVersion(void) {
  return FARMANAGERVERSION;
}

int WINAPI GetMinFarVersionW(void) {
  return FARMANAGERVERSION;
}

void WINAPI SetStartupInfoW(const struct PluginStartupInfo *Info) {
  Far::init(Info);
  g_options.load();
  g_plugin_prefix = g_options.plugin_prefix;
}

void WINAPI GetPluginInfoW(struct PluginInfo *Info) {
  FAR_ERROR_HANDLER_BEGIN;
  static const wchar_t* plugin_menu[1];
  static const wchar_t* config_menu[1];
  plugin_menu[0] = Far::msg_ptr(MSG_PLUGIN_NAME);
  config_menu[0] = Far::msg_ptr(MSG_PLUGIN_NAME);

  Info->StructSize = sizeof(PluginInfo);
  Info->PluginMenuStrings = plugin_menu;
  Info->PluginMenuStringsNumber = ARRAYSIZE(plugin_menu);
  Info->PluginConfigStrings = config_menu;
  Info->PluginConfigStringsNumber = ARRAYSIZE(config_menu);
  Info->CommandPrefix = g_plugin_prefix.c_str();
  FAR_ERROR_HANDLER_END(return, return, false);
}

HANDLE WINAPI OpenPluginW(int OpenFrom, INT_PTR Item) {
  FAR_ERROR_HANDLER_BEGIN;
  if (OpenFrom == OPEN_PLUGINSMENU) {
    vector<wstring> menu_items;
    menu_items.push_back(Far::get_msg(MSG_MENU_OPEN));
    menu_items.push_back(Far::get_msg(MSG_MENU_DETECT));
    menu_items.push_back(Far::get_msg(MSG_MENU_EXTRACT));
    int item = Far::menu(Far::get_msg(MSG_PLUGIN_NAME), menu_items);
    if (item == 0 || item == 1) {
      bool auto_detect = item == 0;
      PanelInfo panel_info;
      if (!Far::get_panel_info(PANEL_ACTIVE, panel_info))
        return INVALID_HANDLE_VALUE;
      Far::PanelItem panel_item = Far::get_current_panel_item(PANEL_ACTIVE);
      if (panel_item.file_attributes & FILE_ATTRIBUTE_DIRECTORY)
        return INVALID_HANDLE_VALUE;
      if (!Far::is_real_file_panel(panel_info)) {
        if ((panel_item.file_attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
          Far::post_keys(vector<DWORD>(1, KEY_CTRLPGDN));
          auto_detect_next_time = auto_detect ? triTrue : triFalse;
        }
        return INVALID_HANDLE_VALUE;
      }
      if (panel_item.file_name == L"..")
        return INVALID_HANDLE_VALUE;
      wstring dir = Far::get_panel_dir(PANEL_ACTIVE);
      wstring path = add_trailing_slash(dir) + panel_item.file_name;
      return new Plugin(path, auto_detect);
    }
    else if (item == 2) {
      PanelInfo panel_info;
      if (!Far::get_panel_info(PANEL_ACTIVE, panel_info))
        return INVALID_HANDLE_VALUE;
      if (!Far::is_real_file_panel(panel_info))
        return INVALID_HANDLE_VALUE;
      vector<wstring> file_list;
      file_list.reserve(panel_info.SelectedItemsNumber);
      wstring dir = Far::get_panel_dir(PANEL_ACTIVE);
      for (int i = 0; i < panel_info.SelectedItemsNumber; i++) {
        Far::PanelItem panel_item = Far::get_selected_panel_item(PANEL_ACTIVE, i);
        if ((panel_item.file_attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
          wstring file_path = add_trailing_slash(dir) + panel_item.file_name;
          file_list.push_back(file_path);
        }
      }
      if (file_list.empty())
        return INVALID_HANDLE_VALUE;
      Plugin::bulk_extract(file_list);
    }
  }
  else if (OpenFrom == OPEN_COMMANDLINE) {
    wstring path = unquote(strip(reinterpret_cast<const wchar_t*>(Item)));
    if (!is_absolute_path(path))
      path = Far::get_absolute_path(path);
    return new Plugin(path, true);
  }
  return INVALID_HANDLE_VALUE;
  FAR_ERROR_HANDLER_END(return INVALID_HANDLE_VALUE, return INVALID_HANDLE_VALUE, false);
}

HANDLE WINAPI OpenFilePluginW(const wchar_t *Name,const unsigned char *Data,int DataSize,int OpMode) {
  FAR_ERROR_HANDLER_BEGIN;
  if (Name == nullptr) {
    if (!g_options.handle_create)
      FAIL(E_ABORT);
    return new Plugin();
  }
  else {
    bool auto_detect;
    if (auto_detect_next_time == triUndef) {
      auto_detect = true;
      if (!g_options.handle_commands)
        FAIL(E_ABORT);
      if (g_options.use_include_masks && !Far::match_masks(extract_file_name(Name), g_options.include_masks))
        FAIL(E_ABORT);
      if (g_options.use_exclude_masks && Far::match_masks(extract_file_name(Name), g_options.exclude_masks))
        FAIL(E_ABORT);
    }
    else if (auto_detect_next_time == triTrue)
      auto_detect = true;
    else
      auto_detect = false;
    auto_detect_next_time = triUndef;
    return new Plugin(Name, auto_detect);
  }
  FAR_ERROR_HANDLER_END(return INVALID_HANDLE_VALUE, return INVALID_HANDLE_VALUE, (OpMode & (OPM_SILENT | OPM_FIND)) != 0);
}

void WINAPI ClosePluginW(HANDLE hPlugin) {
  FAR_ERROR_HANDLER_BEGIN;
  Plugin* plugin = reinterpret_cast<Plugin*>(hPlugin);
  IGNORE_ERRORS(plugin->close());
  delete plugin;
  FAR_ERROR_HANDLER_END(return, return, true);
}

void WINAPI GetOpenPluginInfoW(HANDLE hPlugin,struct OpenPluginInfo *Info) {
  FAR_ERROR_HANDLER_BEGIN;
  reinterpret_cast<Plugin*>(hPlugin)->info(Info);
  FAR_ERROR_HANDLER_END(return, return, false);
}

int WINAPI SetDirectoryW(HANDLE hPlugin,const wchar_t *Dir,int OpMode) {
  FAR_ERROR_HANDLER_BEGIN;
  reinterpret_cast<Plugin*>(hPlugin)->set_dir(Dir);
  return TRUE;
  FAR_ERROR_HANDLER_END(return FALSE, return FALSE, (OpMode & (OPM_SILENT | OPM_FIND)) != 0);
}

int WINAPI GetFindDataW(HANDLE hPlugin,struct PluginPanelItem **pPanelItem,int *pItemsNumber,int OpMode) {
  FAR_ERROR_HANDLER_BEGIN;
  reinterpret_cast<Plugin*>(hPlugin)->list(pPanelItem, pItemsNumber);
  return TRUE;
  FAR_ERROR_HANDLER_END(return FALSE, return FALSE, (OpMode & (OPM_SILENT | OPM_FIND)) != 0);
}

void WINAPI FreeFindDataW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber) {
  FAR_ERROR_HANDLER_BEGIN;
  delete[] PanelItem;
  FAR_ERROR_HANDLER_END(return, return, false);
}

int WINAPI GetFilesW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber,int Move,const wchar_t **DestPath,int OpMode) {
  FAR_ERROR_HANDLER_BEGIN
  reinterpret_cast<Plugin*>(hPlugin)->get_files(PanelItem, ItemsNumber, Move, DestPath, OpMode);
  return 1;
  FAR_ERROR_HANDLER_END(return 0, return -1, (OpMode & (OPM_FIND | OPM_QUICKVIEW)) != 0);
}

int WINAPI PutFilesW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber,int Move,const wchar_t *SrcPath,int OpMode) {
  FAR_ERROR_HANDLER_BEGIN;
  reinterpret_cast<Plugin*>(hPlugin)->put_files(PanelItem, ItemsNumber, Move, SrcPath, OpMode);
  return 2;
  FAR_ERROR_HANDLER_END(return 0, return -1, (OpMode & OPM_FIND) != 0);
}

int WINAPI DeleteFilesW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber,int OpMode) {
  FAR_ERROR_HANDLER_BEGIN;
  reinterpret_cast<Plugin*>(hPlugin)->delete_files(PanelItem, ItemsNumber, OpMode);
  return TRUE;
  FAR_ERROR_HANDLER_END(return FALSE, return FALSE, (OpMode & OPM_SILENT) != 0);
}

int WINAPI ProcessHostFileW(HANDLE hPlugin, struct PluginPanelItem *PanelItem, int ItemsNumber, int OpMode) {
  FAR_ERROR_HANDLER_BEGIN;
  vector<wstring> menu_items;
  menu_items.push_back(Far::get_msg(MSG_TEST_MENU));
  int item = Far::menu(Far::get_msg(MSG_PLUGIN_NAME), menu_items);
  if (item == 0)
    reinterpret_cast<Plugin*>(hPlugin)->test_files(PanelItem, ItemsNumber, OpMode);
  return TRUE;
  FAR_ERROR_HANDLER_END(return FALSE, return FALSE, (OpMode & OPM_SILENT) != 0);
}

int WINAPI ProcessKeyW(HANDLE hPlugin,int Key,unsigned int ControlState) {
  FAR_ERROR_HANDLER_BEGIN;
  if (ControlState == PKF_CONTROL && Key == 'A') {
    reinterpret_cast<Plugin*>(hPlugin)->show_attr();
    return TRUE;
  }
  return FALSE;
  FAR_ERROR_HANDLER_END(return FALSE, return FALSE, false);
}

int WINAPI ConfigureW(int ItemNumber) {
  FAR_ERROR_HANDLER_BEGIN;
  PluginSettings settings;
  settings.handle_create = g_options.handle_create;
  settings.handle_commands = g_options.handle_commands;
  settings.use_include_masks = g_options.use_include_masks;
  settings.include_masks = g_options.include_masks;
  settings.use_exclude_masks = g_options.use_exclude_masks;
  settings.exclude_masks = g_options.exclude_masks;
  if (settings_dialog(settings)) {
    g_options.handle_create = settings.handle_create;
    g_options.handle_commands = settings.handle_commands;
    g_options.use_include_masks = settings.use_include_masks;
    g_options.include_masks = settings.include_masks;
    g_options.use_exclude_masks = settings.use_exclude_masks;
    g_options.exclude_masks = settings.exclude_masks;
    g_options.save();
    return TRUE;
  }
  else
    return FALSE;
  FAR_ERROR_HANDLER_END(return FALSE, return FALSE, false);
}

void WINAPI ExitFARW() {
  ArcAPI::free();
}
