/*******************************************************************\

Module: Memory Analyzer

Author: Malte Mues <mail.mues@gmail.com>
        Daniel Poetzl

\*******************************************************************/

/// \file
/// Commandline parser for the memory analyzer executing main work.

#include "memory_analyzer_parse_options.h"

#include <util/config.h>
#include <util/exit_codes.h>
#include <util/help_formatter.h>
#include <util/message.h>
#include <util/version.h>

#include <goto-programs/goto_model.h>
#include <goto-programs/read_goto_binary.h>
#include <goto-programs/show_symbol_table.h>

#include <ansi-c/ansi_c_language.h>
#include <langapi/mode.h>

#include "analyze_symbol.h"

#include <fstream> // IWYU pragma: keep
#include <iostream>

memory_analyzer_parse_optionst::memory_analyzer_parse_optionst(
  int argc,
  const char *argv[])
  : parse_options_baset(
      MEMORY_ANALYZER_OPTIONS,
      argc,
      argv,
      std::string("MEMORY-ANALYZER ") + CBMC_VERSION),
    message(ui_message_handler)
{
}

void memory_analyzer_parse_optionst::register_languages()
{
  // For now only C is supported due to the additional challenges of
  // mapping source code to debugging symbols in other languages.
  register_language(new_ansi_c_language);
}

int memory_analyzer_parse_optionst::doit()
{
  if(cmdline.isset("version"))
  {
    std::cout << CBMC_VERSION << '\n';
    return CPROVER_EXIT_SUCCESS;
  }

  config.set(cmdline);

  if(cmdline.args.size() < 1)
  {
    throw invalid_command_line_argument_exceptiont(
      "no binary provided for analysis", "<binary> <args>");
  }

  if(!cmdline.isset("symbols"))
  {
    throw invalid_command_line_argument_exceptiont(
      "need to provide symbols to analyse via --symbols", "--symbols");
  }

  const bool core_file = cmdline.isset("core-file");
  const bool breakpoint = cmdline.isset("breakpoint");

  if(core_file && breakpoint)
  {
    throw invalid_command_line_argument_exceptiont(
      "cannot start gdb from both core-file and breakpoint",
      "--core-file/--breakpoint");
  }

  if(!core_file && !breakpoint)
  {
    throw invalid_command_line_argument_exceptiont(
      "need to provide either core-file or breakpoint for gdb",
      "--core-file/--breakpoint");
  }

  const bool output_file = cmdline.isset("output-file");
  const bool symtab_snapshot = cmdline.isset("symtab-snapshot");

  if(symtab_snapshot && output_file)
  {
    throw invalid_command_line_argument_exceptiont(
      "printing to a file is not supported for symbol table snapshot output",
      "--symtab-snapshot");
  }

  register_languages();

  std::string binary = cmdline.args.front();

  auto opt = read_goto_binary(binary, ui_message_handler);

  if(!opt.has_value())
  {
    throw deserialization_exceptiont(
      "cannot read goto binary '" + binary + "'");
  }

  const goto_modelt goto_model(std::move(opt.value()));

  gdb_value_extractort gdb_value_extractor(
    goto_model.symbol_table, cmdline.args);
  gdb_value_extractor.create_gdb_process();

  if(core_file)
  {
    std::string core_file = cmdline.get_value("core-file");
    gdb_value_extractor.run_gdb_from_core(core_file);
  }
  else if(breakpoint)
  {
    std::string breakpoint = cmdline.get_value("breakpoint");
    gdb_value_extractor.run_gdb_to_breakpoint(breakpoint);
  }

  gdb_value_extractor.analyze_symbols(
    cmdline.get_comma_separated_values("symbols"));

  std::ofstream file;

  if(output_file)
  {
    file.open(cmdline.get_value("output-file"));
  }

  std::ostream &out =
    output_file ? (std::ostream &)file : (std::ostream &)message.result();

  if(symtab_snapshot)
  {
    symbol_tablet snapshot = gdb_value_extractor.get_snapshot_as_symbol_table();
    show_symbol_table(snapshot, ui_message_handler);
  }
  else
  {
    std::string snapshot = gdb_value_extractor.get_snapshot_as_c_code();
    out << snapshot;
  }

  if(output_file)
  {
    file.close();
  }
  else
  {
    message.result() << messaget::eom;
  }

  return CPROVER_EXIT_SUCCESS;
}

void memory_analyzer_parse_optionst::help()
{
  std::cout << '\n'
            << banner_string("Memory-Analyzer", CBMC_VERSION) << '\n'
            << align_center_with_border("Copyright (C) 2019") << '\n'
            << align_center_with_border("Malte Mues, Diffblue Ltd.") << '\n'
            << align_center_with_border("info@diffblue.com") << '\n';

  // clang-format off
  std::cout << help_formatter(
    "\n"
    "Usage:                     \tPurpose:\n"
    "\n"
    " {bmemory-analyzer} [{y-?}] [{y-h}] [{y--help}] \t show this help\n"
    " {bmemory-analyzer} {y--version} \t show version\n"
    " {bmemory-analyzer} [options] {y--symbols} {usymbol-list} {ubinary} \t"
    " analyze {ubinary}\n"
    "\n"
    "Main options:\n"
    " {y--core-file} {ufile_name} \t analyze from core file {ufile_name}\n"
    " {y--breakpoint {uname} \t analyze from breakpoint {uname}\n"
    " {y--symbols {usymbol-list} \t list of symbols to analyze\n"
    " {y--symtab-snapshot} \t output snapshot as symbol table\n"
    " {y--output-file} {ufile_name} \t write snapshot to file {ufile_name}\n"
    " {y--json-ui} \t output snapshot in JSON format\n"
    "\n");
  // clang-format on
}
