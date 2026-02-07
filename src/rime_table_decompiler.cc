// rime_table_decompiler.cc
// nopdan <me@nopdan.com>
//
#ifndef RIME_WASM
#include <codepage.h>
#endif
#include <rime/dict/table.h>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>

// usage:
//   rime_table_decompiler <rime-table-file> [save-path]
// example:
//   rime_table_decompiler pinyin.table.bin pinyin.dict.yaml

void outCode(rime::Table* table, const rime::Code code, std::ostream& out) {
  if (code.empty()) {
    return;
  }
  auto item = code.begin();
  out << table->GetSyllableById(*item);
  item++;
  for (; item != code.end(); ++item) {
    out << " ";
    out << table->GetSyllableById(*item);
  }
  return;
}

void access(rime::Table* table,
            rime::TableAccessor accessor,
            std::ostream& out) {
  while (!accessor.exhausted()) {
    auto word = table->GetEntryText(*accessor.entry());
    out << word << "\t";
    outCode(table, accessor.code(), out);

    auto weight = accessor.entry()->weight;
#ifdef TABLE_V3
    if (weight > 0) {
      out << "\t" << weight;
    }
#else
    if (weight >= 0) {
      out << "\t" << exp(weight);
    }
#endif
    out << std::endl;
    accessor.Next();
  }
}

// recursively traverse table
void recursion(rime::Table* table,
               rime::TableQuery* query,
               std::ostream& out) {
  for (int i = 0; i < table->metadata()->num_syllables; i++) {
    auto accessor = query->Access(i);
    access(table, accessor, out);
    if (query->Advance(i)) {
      if (query->level() < 3) {
        recursion(table, query, out);
      } else {
        auto accessor = query->Access(0);
        access(table, accessor, out);
      }
      query->Backdate();
    }
  }
}

void traversal(rime::Table* table, std::ostream& out) {
  auto metadata = table->metadata();
#ifndef RIME_WASM
  std::cout << "num_syllables: " << metadata->num_syllables << std::endl;
  std::cout << "num_entries: " << metadata->num_entries << std::endl;
#endif

  rime::TableQuery query(table->metadata()->index.get());
  recursion(table, &query, out);
}

void writeHeader(const std::string& name, std::ostream& out) {
  out << "# Rime dictionary\n\n";
  out << "---\n"
         "name: "
      << name
      << "\n"
         "version: \"1.0\"\n"
         "...\n\n";
}

bool decompileTable(rime::Table* table,
                    const std::string& name,
                    std::ostream& out) {
  out << std::fixed;
  out << std::setprecision(0);
  writeHeader(name, out);
  traversal(table, out);
  return true;
}

#ifdef RIME_WASM
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

extern "C" {

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
uint8_t* decompile(const uint8_t* data, size_t size, size_t* out_size) {
  if (!out_size) {
    return nullptr;
  }
  *out_size = 0;
  if (!data || size == 0) {
    return nullptr;
  }

  rime::Table table("<memory>");
  if (!table.LoadFromMemory(data, size)) {
    return nullptr;
  }

  std::ostringstream out;
  decompileTable(&table, "rime_table", out);
  std::string output = out.str();
  if (output.empty()) {
    return nullptr;
  }

  auto* buffer = static_cast<uint8_t*>(std::malloc(output.size()));
  if (!buffer) {
    return nullptr;
  }
  std::memcpy(buffer, output.data(), output.size());
  *out_size = output.size();
  return buffer;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void decompile_free(uint8_t* ptr) {
  std::free(ptr);
}

}  // extern "C"
#endif

#ifndef RIME_WASM
int main(int argc, char* argv[]) {
  unsigned int codepage = SetConsoleOutputCodePage();
  if (argc < 2 || argc > 3) {
    std::cout << "Usage: rime_table_decompiler <rime-table-file> [save-path]"
              << std::endl;
    std::cout
        << "Example: rime_table_decompiler pinyin.table.bin pinyin.dict.yaml"
        << std::endl;
    SetConsoleOutputCodePage(codepage);
    return 0;
  }

#ifdef RIME_ENABLE_LOGGING
  google::InitGoogleLogging(argv[0]);
#endif
  std::string fileName(argv[1]);
  rime::Table table(fileName);
  bool success = table.Load();
  if (!success) {
    std::cerr << "Failed to load table." << std::endl;
    system("pause");
    SetConsoleOutputCodePage(codepage);
    return 1;
  }

  // Remove the extension ".table.bin" if present.
  const size_t table_bin_idx = fileName.rfind(".table.bin");
  if (std::string::npos != table_bin_idx) {
    fileName.erase(table_bin_idx);
  }
  const std::string outputName = (argc == 3) ? argv[2] : fileName + ".yaml";

  std::ofstream fout;
  fout.open(outputName);
  if (!fout.is_open()) {
    std::cerr << "Failed to open file " << outputName << std::endl;
    system("pause");
    SetConsoleOutputCodePage(codepage);
    return 1;
  }

  // schema id
  const size_t last_slash_idx = fileName.find_last_of("\\/");
  if (std::string::npos != last_slash_idx) {
    fileName.erase(0, last_slash_idx + 1);
  }
  decompileTable(&table, fileName, fout);
  fout.close();
  std::cout << "Save to: " << outputName << std::endl
            << std::endl
            << "Author: nopdan <me@nopdan.com>" << std::endl
            << "Date: 2023-12-12" << std::endl
            << "Repo: https://github.com/nopdan/rime-table-decompiler"
            << std::endl;
  system("pause");
  SetConsoleOutputCodePage(codepage);
  return 0;
}
#endif