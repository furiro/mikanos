/**
 * @file terminal.hpp
 *
 * ターミナルウィンドウを提供する。
 */

#pragma once

#include <array>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include "window.hpp"
#include "task.hpp"
#include "layer.hpp"
#include "fat.hpp"
#include "graphics.hpp"
#include "elf.hpp"

typedef struct elf64_sym {
  Elf64_Word st_name;		/* Symbol name, index in string tbl */
  unsigned char	st_info;	/* Type and binding attributes */
  unsigned char	st_other;	/* No defined meaning, 0 */
  Elf64_Half st_shndx;		/* Associated section index */
  Elf64_Addr st_value;		/* Value of the symbol */
  Elf64_Xword st_size;		/* Associated symbol size */
} Elf64_Sym;

typedef struct {
  Elf64_Addr  lib_base;
  Elf64_Sym*  symbol_table_addres;  // DT_SYMTAB
  char*       str_table_addres;     // DT_STRTAB
  size_t      symbol_count;
} LibraryInfo;


#define DT_NULL		0
#define DT_NEEDED	1
#define DT_PLTRELSZ	2
#define DT_PLTGOT	3
#define DT_HASH		4
#define DT_STRTAB	5
#define DT_SYMTAB	6
#define DT_RELA		7
#define DT_RELASZ	8
#define DT_RELAENT	9
#define DT_STRSZ	10
#define DT_SYMENT	11
#define DT_INIT		12
#define DT_FINI		13
#define DT_SONAME	14
#define DT_RPATH 	15
#define DT_SYMBOLIC	16
#define DT_REL      17
#define DT_RELSZ	18
#define DT_RELENT	19
#define DT_PLTREL	20
#define DT_DEBUG	21
#define DT_TEXTREL	22
#define DT_JMPREL	23
#define DT_ENCODING	32






struct AppLoadInfo {
  uint64_t vaddr_end, entry;
  PageMapEntry* pml4;
};

extern std::map<fat::DirectoryEntry*, AppLoadInfo>* app_loads;

struct TerminalDescriptor {
  std::string command_line;
  bool exit_after_command;
  bool show_window;
  std::array<std::shared_ptr<FileDescriptor>, 3> files;
};

enum class EscSeqState {
  kInit, // エスケープシーケンスに出会ってない状態
  kEsc,  // ESC を受信した直後の状態
  kCSI,  // \033[
  kNum,  // 数字を 1 文字以上受信した状態
};

class Terminal {
 public:
  static const int kRows = 15, kColumns = 60;
  static const int kLineMax = 128;

  Terminal(Task& task, const TerminalDescriptor* term_desc);
  unsigned int LayerID() const { return layer_id_; }
  Rectangle<int> BlinkCursor();
  Rectangle<int> InputKey(uint8_t modifier, uint8_t keycode, char ascii);

  void Print(const char* s, std::optional<size_t> len = std::nullopt);

  Task& UnderlyingTask() const { return task_; }
  int LastExitCode() const { return last_exit_code_; }
  void Redraw();
  

 private:
  std::shared_ptr<ToplevelWindow> window_;
  unsigned int layer_id_;
  Task& task_;

  Vector2D<int> cursor_{0, 0};
  bool cursor_visible_{false};
  void DrawCursor(bool visible);
  Vector2D<int> CalcCursorPos() const;

  int linebuf_index_{0};
  std::array<char, kLineMax> linebuf_{};
  void Scroll1();

  void ExecuteLine();
  WithError<int> ExecuteFile(fat::DirectoryEntry& file_entry,
                             char* command, char* first_arg);
  void Print(char32_t c);

  std::deque<std::array<char, kLineMax>> cmd_history_{};
  int cmd_history_index_{-1};
  Rectangle<int> HistoryUpDown(int direction);

  bool show_window_;
  std::array<std::shared_ptr<FileDescriptor>, 3> files_;
  int last_exit_code_{0};

  EscSeqState esc_seq_state_{EscSeqState::kInit};
  int esc_seq_n_{0};
  PixelColor text_color_{255, 255, 255};
};

void TaskTerminal(uint64_t task_id, int64_t data);

class TerminalFileDescriptor : public FileDescriptor {
 public:
  explicit TerminalFileDescriptor(Terminal& term);
  size_t Read(void* buf, size_t len) override;
  size_t Write(const void* buf, size_t len) override;
  size_t Size() const override { return 0; }
  bool IsTerminal() const override { return true; }
  size_t Load(void* buf, size_t len, size_t offset) override;

 private:
  Terminal& term_;
};

class PipeDescriptor : public FileDescriptor {
 public:
  explicit PipeDescriptor(Task& task);
  size_t Read(void* buf, size_t len) override;
  size_t Write(const void* buf, size_t len) override;
  size_t Size() const override { return 0; }
  size_t Load(void* buf, size_t len, size_t offset) override { return 0; }

  void FinishWrite();

 private:
  Task& task_;
  char data_[16];
  size_t len_{0};
  bool closed_{false};
};


