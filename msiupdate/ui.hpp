#pragma once

void error_dlg(const Error& e);
void error_dlg(const std::exception& e);
void error_dlg(const wstring& msg);
void info_dlg(const wstring& msg);

class ProgressMonitor {
private:
  HANDLE h_scr;
  wstring con_title;
  unsigned __int64 t_start;
  unsigned __int64 t_curr;
  unsigned __int64 t_next;
  unsigned __int64 t_freq;
protected:
  unsigned __int64 time_elapsed() const {
    return (t_curr - t_start) / t_freq;
  }
  virtual void do_update_ui() = 0;
public:
  ProgressMonitor(bool lazy = true);
  virtual ~ProgressMonitor();
  void update_ui(bool force = false);
};

bool config_dialog(Options& options);

const wchar_t** get_size_suffixes();
const wchar_t** get_speed_suffixes();