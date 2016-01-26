/*
 * cli.h
 *
 *  Created on: Jan 26, 2016
 *      Author: petera
 */

#ifndef CLI_H_
#define CLI_H_

#include "system.h"

#define CLI_OK                      0

// indentation between command and help string
#define CLI_HELP_INDENT             32
// maximum tree depth of menus
#define CLI_MAX_SUBMENU_DEPTH       8
// maximum characters in a command line
#define CLI_MAX_LINE                512
// maximum arguments in a command line
#define CLI_MAX_ARGS                16


// string of characters that are ignored during parsing
#define CLI_FORMAT_CHARS_IGNORE     "\r"
// argument delimiter character
#define CLI_FORMAT_CHAR_DELIM       ' '
// command end character
#define CLI_FORMAT_CHAR_END         '\n'


// cli output macro
#define CLI_PRINTF(fmt, ...)        print(fmt,  ## __VA_ARGS__ )


#ifndef CONFIG_CLI
#define CLI_EXTERN_MENU(_menusym)
#define CLI_MENU_START_MAIN
#define CLI_MENU_START(_menusym)
#define CLI_FUNC(_name, _fn, _helptext)
#define CLI_EXTRAMENU(_menusym)
#define CLI_SUBMENU(_menusym, _name, _helptext)
#define CLI_MENU_END
#else
// extern declare another menu from some other file
#define CLI_EXTERN_MENU(_menusym) \
  extern const cli_cmd _cli_menu_ ## _menusym[];
// top level main menu start declaration
#define CLI_MENU_START_MAIN \
  CLI_MENU_START(main)
// menu start declaration
#define CLI_MENU_START(_menusym) \
  const cli_cmd _cli_menu_ ## _menusym[] = {
// client function command
#define CLI_FUNC(_name, _fn, _helptext) \
  (const cli_cmd){ .name=(char *)(_name), .type=CLI_FUNC, .fn=(cli_func)(_fn), .help=(char *)(_helptext) },
// add commands from another menu to wrapping menu (commands from extramenu will be directly displayed in parent)
#define CLI_EXTRAMENU(_menusym) \
  (const cli_cmd){ .name=(char *)(""), .type=CLI_EXTRA, .submenu=(const cli_cmd *)(_cli_menu_ ## _menusym), .help=(char *)("") },
// add commands from another menu as a submenu (only the submenu name will be displayed in parent)
#define CLI_SUBMENU(_menusym, _name, _helptext) \
  (const cli_cmd){ .name=(char *)(_name), .type=CLI_SUB, .submenu=(const cli_cmd *)(_cli_menu_ ## _menusym), .help=(char *)(_helptext) },
// menu end declaration
#define CLI_MENU_END \
  ( (const cli_cmd){ .name=NULL, .fn=NULL, .help=NULL } ) };
#endif // CONFIG_CLI


// cli function signature
typedef s32_t (*cli_func)(u32_t args, ...);

typedef enum {
  CLI_FUNC = 0,
  CLI_SUB,
  CLI_EXTRA
} cli_cmd_type_e;

typedef struct _cli_cmd {
  char *name;
  cli_cmd_type_e type;
  union {
    cli_func fn;
    const struct _cli_cmd *submenu;
  };
  char *help;
} cli_cmd;


// initiates the cli
void cli_init(void);

// pass the cli characters to work with
void cli_parse(char *s, u32_t len);

// generic cli help function, supposedly used in a CLI_FUNC macro within the main menu
// Takes zero or more arguments:
// - on zero arguments, the commands of top level tree is printed
// - on one argument being "all", the command tree is printed recursively
// - on other arguments, the help for command indicated by arguments is printed
s32_t cli_help(u32_t args, ...);


#endif /* CLI_H_ */
