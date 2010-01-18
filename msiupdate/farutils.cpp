#include "utils.hpp"
#include "farutils.hpp"
#include "options.hpp"
#include "ui.hpp"

namespace Far {

PluginStartupInfo g_far;
FarStandardFunctions g_fsf;

void init(const PluginStartupInfo* psi) {
  g_far = *psi;
  g_fsf = *psi->FSF;
}

wstring get_plugin_module_path() {
  return extract_file_path(g_far.ModuleName);
}

wstring get_root_key_name() {
  return g_far.RootKey;
}

unsigned get_version() {
  DWORD version;
  g_far.AdvControl(g_far.ModuleNumber, ACTL_GETFARVERSION, &version);
  return (LOWORD(version) << 16) | HIWORD(version);
}

const wchar_t* msg_ptr(int id) {
  return g_far.GetMsg(g_far.ModuleNumber, id);
}

wstring get_msg(int id) {
  return g_far.GetMsg(g_far.ModuleNumber, id);
}

unsigned get_optimal_msg_width() {
  HANDLE con = GetStdHandle(STD_OUTPUT_HANDLE);
  if (con != INVALID_HANDLE_VALUE) {
    CONSOLE_SCREEN_BUFFER_INFO con_info;
    if (GetConsoleScreenBufferInfo(con, &con_info)) {
      unsigned con_width = con_info.srWindow.Right - con_info.srWindow.Left + 1;
      if (con_width >= 80)
        return con_width - 20;
    }
  }
  return 60;
}

int message(const wstring& msg, int button_cnt, DWORD flags) {
  return g_far.Message(g_far.ModuleNumber, flags | FMSG_ALLINONE, NULL, reinterpret_cast<const wchar_t* const*>(msg.c_str()), 0, button_cnt);
}

wstring get_progress_bar_str(unsigned width, unsigned __int64 completed, unsigned __int64 total) {
  const wchar_t c_pb_black = 9608;
  const wchar_t c_pb_white = 9617;
  unsigned len1;
  if (total == 0)
    len1 = 0;
  else
    len1 = static_cast<unsigned>(static_cast<double>(completed) * width / total);
  if (len1 > width)
    len1 = width;
  unsigned len2 = width - len1;
  wstring result;
  result.append(len1, c_pb_black);
  result.append(len2, c_pb_white);
  return result;
}

void set_progress_state(TBPFLAG state) {
  g_far.AdvControl(g_far.ModuleNumber, ACTL_SETPROGRESSSTATE, reinterpret_cast<void*>(state));
}

void set_progress_value(unsigned __int64 completed, unsigned __int64 total) {
  PROGRESSVALUE pv;
  pv.Completed = completed;
  pv.Total = total;
  g_far.AdvControl(g_far.ModuleNumber, ACTL_SETPROGRESSVALUE, &pv);
}

void call_user_apc(void* param) {
  g_far.AdvControl(g_far.ModuleNumber, ACTL_SYNCHRO, param);
}

bool post_keys(const vector<DWORD>& keys) {
  KeySequence key_sequence;
  key_sequence.Flags = KSFLAGS_DISABLEOUTPUT;
  key_sequence.Count = static_cast<int>(keys.size());
  key_sequence.Sequence = to_array(keys);
  return g_far.AdvControl(g_far.ModuleNumber, ACTL_POSTKEYSEQUENCE, &key_sequence) == TRUE;
}

FarConfirmationsSettings get_confirmation_settings() {
  return static_cast<FarConfirmationsSettings>(g_far.AdvControl(g_far.ModuleNumber, ACTL_GETCONFIRMATIONS, NULL));
}

bool get_short_window_info(unsigned idx, WindowInfo& window_info) {
  window_info.Pos = idx;
  return g_far.AdvControl(g_far.ModuleNumber, ACTL_GETSHORTWINDOWINFO, &window_info) == TRUE;
}

HANDLE save_screen() {
  return g_far.SaveScreen(0, 0, -1, -1);
}

void restore_screen(HANDLE h_scr) {
  g_far.RestoreScreen(h_scr);
}

void flush_screen() {
  g_far.Text(0, 0, 0, NULL); // flush buffer hack
  g_far.AdvControl(g_far.ModuleNumber, ACTL_REDRAWALL, 0);
}

int viewer(const wstring& file_name, const wstring& title) {
  return g_far.Viewer(file_name.c_str(), title.c_str(), 0, 0, -1, -1, VF_DISABLEHISTORY | VF_ENABLE_F6, CP_UNICODE);
}

unsigned get_label_len(const wstring& str) {
  unsigned cnt = 0;
  for (unsigned i = 0; i < str.size(); i++) {
    if (str[i] != '&') cnt++;
  }
  return cnt;
}

unsigned Dialog::new_value(const wstring& text) {
  values.push_back(text);
  return static_cast<unsigned>(values.size());
}

const wchar_t* Dialog::get_value(unsigned idx) const {
  return values[idx - 1].c_str();
}

void Dialog::frame(const wstring& text) {
  DialogItem di;
  di.type = DI_DOUBLEBOX;
  di.x1 = c_x_frame - 2;
  di.y1 = c_y_frame - 1;
  di.x2 = c_x_frame + client_xs + 1;
  di.y2 = c_y_frame + client_ys;
  di.text_idx = new_value(text);
  new_item(di);
}

void Dialog::calc_frame_size() {
  client_ys = y - c_y_frame;
  DialogItem& di = items.front(); // dialog frame
  di.x2 = c_x_frame + client_xs + 1;
  di.y2 = c_y_frame + client_ys;
}

unsigned Dialog::new_item(const DialogItem& di) {
  items.push_back(di);
  return static_cast<unsigned>(items.size()) - 1;
}

LONG_PTR WINAPI Dialog::internal_dialog_proc(HANDLE h_dlg, int msg, int param1, LONG_PTR param2) {
  Dialog* dlg = reinterpret_cast<Dialog*>(g_far.SendDlgMessage(h_dlg, DM_GETDLGDATA, 0, 0));
  dlg->h_dlg = h_dlg;
  FAR_ERROR_HANDLER_BEGIN;
  return dlg->dialog_proc(msg, param1, param2);
  FAR_ERROR_HANDLER_END( , , false);
  return dlg->default_dialog_proc(msg, param1, param2);
}

LONG_PTR Dialog::default_dialog_proc(int msg, int param1, LONG_PTR param2) {
  return g_far.DefDlgProc(h_dlg, msg, param1, param2);
}

Dialog::Dialog(const wstring& title, unsigned width, const wchar_t* help): client_xs(width), x(c_x_frame), y(c_y_frame), help(help) {
  frame(title);
}

void Dialog::new_line() {
  x = c_x_frame;
  y++;
}

void Dialog::spacer(unsigned size) {
  x += size;
  if (x - c_x_frame > client_xs)
    client_xs = x - c_x_frame;
}

unsigned Dialog::separator() {
  DialogItem di;
  di.type = DI_TEXT;
  di.y1 = y;
  di.y2 = y;
  di.flags = DIF_SEPARATOR;
  return new_item(di);
}

unsigned Dialog::label(const wstring& text, unsigned boxsize, DWORD flags) {
  DialogItem di;
  di.type = DI_TEXT;
  di.x1 = x;
  di.y1 = y;
  if (boxsize == AUTO_SIZE)
    x += get_label_len(text);
  else
    x += boxsize;
  if (x - c_x_frame > client_xs)
    client_xs = x - c_x_frame;
  di.x2 = x - 1;
  di.y2 = y;
  di.flags = flags;
  di.text_idx = new_value(text);
  return new_item(di);
}

unsigned Dialog::edit_box(const wstring& text, unsigned boxsize, DWORD flags) {
  DialogItem di;
  di.type = DI_EDIT;
  di.x1 = x;
  di.y1 = y;
  if (boxsize == AUTO_SIZE)
    x = c_x_frame + client_xs;
  else
    x += boxsize;
  if (x - c_x_frame > client_xs)
    client_xs = x - c_x_frame;
  di.x2 = x - 1;
  di.y2 = y;
  di.flags = flags;
  di.text_idx = new_value(text);
  return new_item(di);
}

unsigned Dialog::mask_edit_box(const wstring& text, const wstring& mask, unsigned boxsize, DWORD flags) {
  DialogItem di;
  di.type = DI_FIXEDIT;
  di.x1 = x;
  di.y1 = y;
  if (boxsize == AUTO_SIZE)
    x += static_cast<unsigned>(mask.size());
  else
    x += boxsize;
  if (x - c_x_frame > client_xs)
    client_xs = x - c_x_frame;
  di.x2 = x - 1;
  di.y2 = y;
  di.mask_idx = new_value(mask);
  di.flags = DIF_MASKEDIT | flags;
  di.text_idx = new_value(text);
  return new_item(di);
}

unsigned Dialog::fix_edit_box(const wstring& text, unsigned boxsize, DWORD flags) {
  DialogItem di;
  di.type = DI_FIXEDIT;
  di.x1 = x;
  di.y1 = y;
  if (boxsize == AUTO_SIZE)
    x += static_cast<unsigned>(text.size());
  else
    x += boxsize;
  if (x - c_x_frame > client_xs)
    client_xs = x - c_x_frame;
  di.x2 = x - 1;
  di.y2 = y;
  di.flags = flags;
  di.text_idx = new_value(text);
  return new_item(di);
}

unsigned Dialog::button(const wstring& text, DWORD flags, bool def) {
  DialogItem di;
  di.type = DI_BUTTON;
  di.x1 = x;
  di.y1 = y;
  x += get_label_len(text) + 4;
  if (x - c_x_frame > client_xs)
    client_xs = x - c_x_frame;
  di.y2 = y;
  di.flags = flags;
  di.default_button = def ? 1 : 0;
  di.text_idx = new_value(text);
  return new_item(di);
}

unsigned Dialog::check_box(const wstring& text, int value, DWORD flags) {
  DialogItem di;
  di.type = DI_CHECKBOX;
  di.x1 = x;
  di.y1 = y;
  x += get_label_len(text) + 4;
  if (x - c_x_frame > client_xs)
    client_xs = x - c_x_frame;
  di.y2 = y;
  di.flags = flags;
  di.selected = value;
  di.text_idx = new_value(text);
  return new_item(di);
}

unsigned Dialog::radio_button(const wstring& text, bool value, DWORD flags) {
  DialogItem di;
  di.type = DI_RADIOBUTTON;
  di.x1 = x;
  di.y1 = y;
  x += get_label_len(text) + 4;
  if (x - c_x_frame > client_xs)
    client_xs = x - c_x_frame;
  di.y2 = y;
  di.flags = flags;
  di.selected = value ? 1 : 0;
  di.text_idx = new_value(text);
  return new_item(di);
}

unsigned Dialog::combo_box(const vector<wstring>& list_items, unsigned sel_idx, unsigned boxsize, DWORD flags) {
  DialogItem di;
  di.type = DI_COMBOBOX;
  di.x1 = x;
  di.y1 = y;
  if (boxsize == AUTO_SIZE) {
    if (flags & DIF_DROPDOWNLIST) {
      unsigned max_len = 1;
      for (unsigned i = 0; i < list_items.size(); i++) {
        if (max_len < list_items[i].size())
          max_len = static_cast<unsigned>(list_items[i].size());
      }
      x += max_len + 1;
    }
    else
      x = c_x_frame + client_xs;
  }
  else
    x += boxsize;
  if (x - c_x_frame > client_xs)
    client_xs = x - c_x_frame;
  di.x2 = x - 1 - 1; // -1 for down arrow
  di.y2 = y;
  di.flags = flags;
  for (unsigned i = 0; i < list_items.size(); i++) {
    if (di.list_idx)
      new_value(list_items[i]);
    else
      di.list_idx = new_value(list_items[i]);
  }
  di.list_size = static_cast<unsigned>(list_items.size());
  di.list_pos = sel_idx;
  return new_item(di);
}

int Dialog::show() {
  calc_frame_size();

  unsigned list_cnt = 0;
  unsigned list_item_cnt = 0;
  for (unsigned i = 0; i < items.size(); i++) {
    if (items[i].list_idx) {
      list_cnt++;
      list_item_cnt += items[i].list_size;
    }
  }
  Buffer<FarList> far_lists(list_cnt);
  far_lists.clear();
  Buffer<FarListItem> far_list_items(list_item_cnt);
  far_list_items.clear();

  Buffer<FarDialogItem> dlg_items(items.size());
  dlg_items.clear();
  unsigned fl_idx = 0;
  unsigned fli_idx = 0;
  for (unsigned i = 0; i < items.size(); i++) {
    FarDialogItem* dlg_item = dlg_items.data() + i;
    dlg_item->Type = items[i].type;
    dlg_item->X1 = items[i].x1;
    dlg_item->Y1 = items[i].y1;
    dlg_item->X2 = items[i].x2;
    dlg_item->Y2 = items[i].y2;
    dlg_item->Focus = items[i].focus;
    dlg_item->Flags = items[i].flags;
    dlg_item->DefaultButton = items[i].default_button;
    dlg_item->Selected = items[i].selected;
    if (items[i].history_idx)
      dlg_item->History = get_value(items[i].history_idx);
    if (items[i].mask_idx)
      dlg_item->Mask = get_value(items[i].mask_idx);
    if (items[i].text_idx)
      dlg_item->PtrData = get_value(items[i].text_idx);
    if (items[i].list_idx) {
      FarList* fl = far_lists.data() + fl_idx;
      fl->Items = far_list_items.data() + fli_idx;
      fl->ItemsNumber = items[i].list_size;
      for (unsigned j = 0; j < items[i].list_size; j++) {
        FarListItem* fli = far_list_items.data() + fli_idx;
        if (j == items[i].list_pos)
          fli->Flags = LIF_SELECTED;
        fli->Text = get_value(items[i].list_idx + j);
        fli_idx++;
      }
      fl_idx++;
      dlg_item->ListItems = fl;
    }
  }

  int res = -1;
  HANDLE h_dlg = g_far.DialogInit(g_far.ModuleNumber, -1, -1, client_xs + 2 * c_x_frame, client_ys + 2 * c_y_frame, help, dlg_items.data(), static_cast<unsigned>(dlg_items.size()), 0, 0, internal_dialog_proc, reinterpret_cast<LONG_PTR>(this));
  if (h_dlg != INVALID_HANDLE_VALUE) {
    res = g_far.DialogRun(h_dlg);
    g_far.DialogFree(h_dlg);
  }
  return res;
}

wstring Dialog::get_text(unsigned ctrl_id) {
  size_t len = g_far.SendDlgMessage(h_dlg, DM_GETTEXTLENGTH, ctrl_id, 0);
  Buffer<wchar_t> buf(len + 1);
  g_far.SendDlgMessage(h_dlg, DM_GETTEXTPTR, ctrl_id, reinterpret_cast<LONG_PTR>(buf.data()));
  return wstring(buf.data(), len);
}

void Dialog::set_text(unsigned ctrl_id, const wstring& text) {
  g_far.SendDlgMessage(h_dlg, DM_SETTEXTPTR, ctrl_id, reinterpret_cast<LONG_PTR>(text.c_str()));
}

bool Dialog::get_check(unsigned ctrl_id) {
  return DlgItem_GetCheck(g_far, h_dlg, ctrl_id) == BSTATE_CHECKED;
}

void Dialog::set_check(unsigned ctrl_id, bool check) {
  g_far.SendDlgMessage(h_dlg, DM_SETCHECK, ctrl_id, check ? BSTATE_CHECKED : BSTATE_UNCHECKED);
}

unsigned Dialog::get_list_pos(unsigned ctrl_id) {
  return static_cast<unsigned>(g_far.SendDlgMessage(h_dlg, DM_LISTGETCURPOS, ctrl_id, 0));
}

void Dialog::set_color(unsigned ctrl_id, unsigned char color) {
  size_t size = g_far.SendDlgMessage(h_dlg, DM_GETDLGITEM, ctrl_id, NULL);
  Buffer<unsigned char> buf(size);
  FarDialogItem* dlg_item = reinterpret_cast<FarDialogItem*>(buf.data());
  g_far.SendDlgMessage(h_dlg, DM_GETDLGITEM, ctrl_id, reinterpret_cast<LONG_PTR>(dlg_item));
  dlg_item->Flags &= ~DIF_COLORMASK;
  dlg_item->Flags |= DIF_SETCOLOR | color;
  g_far.SendDlgMessage(h_dlg, DM_SETDLGITEM, ctrl_id, reinterpret_cast<LONG_PTR>(dlg_item));
}

void Dialog::set_focus(unsigned ctrl_id) {
  g_far.SendDlgMessage(h_dlg, DM_SETFOCUS, ctrl_id, 0);
}

void Dialog::enable(unsigned ctrl_id, bool enable) {
  g_far.SendDlgMessage(h_dlg, DM_ENABLE, ctrl_id, enable ? TRUE : FALSE);
}

Regex::Regex(): h_regex(INVALID_HANDLE_VALUE) {
  CHECK(g_far.RegExpControl(0, RECTL_CREATE, reinterpret_cast<LONG_PTR>(&h_regex)));
}

Regex::~Regex() {
  if (h_regex != INVALID_HANDLE_VALUE)
    CHECK(g_far.RegExpControl(h_regex, RECTL_FREE, 0));
}

size_t Regex::search(const wstring& expr, const wstring& text) {
  CHECK(g_far.RegExpControl(h_regex, RECTL_COMPILE, reinterpret_cast<LONG_PTR>((L"/" + expr + L"/").c_str())));
  CHECK(g_far.RegExpControl(h_regex, RECTL_OPTIMIZE, 0));
  RegExpSearch regex_search;
  memset(&regex_search, 0, sizeof(regex_search));
  regex_search.Text = text.c_str();
  regex_search.Position = 0;
  regex_search.Length = static_cast<int>(text.size());
  RegExpMatch regex_match;
  regex_search.Match = &regex_match;
  regex_search.Count = 1;
  if (g_far.RegExpControl(h_regex, RECTL_SEARCHEX, reinterpret_cast<LONG_PTR>(&regex_search)))
    return regex_search.Match[0].start;
  else
    return -1;
}

};