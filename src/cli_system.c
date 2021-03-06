/*
 * cli_system.c
 *
 *  Created on: Jan 29, 2016
 *      Author: petera
 */

#include "cli.h"
#include "linker_symaccess.h"
#include "taskq.h"
#include "miniutils.h"
#include <stdarg.h>
#ifdef CONFIG_OS
#include "os.h"
#endif

#ifdef DBG_OFF
static int cli_dbg(u32_t argc) {
  print("Debug disabled compile-time\n");
  return CLI_OK;
}
#else
const char* DBG_BIT_NAME[] = _DBG_BIT_NAMES;

static void print_debug_setting(void) {
  print("DBG level: %i\n", SYS_dbg_get_level());
  int d;
  for (d = 0; d < sizeof(DBG_BIT_NAME) / sizeof(const char*); d++) {
    print("DBG mask %s: %s\n", DBG_BIT_NAME[d],
        SYS_dbg_get_mask() & (1 << d) ? "ON" : "OFF");
  }
}

static int cli_dbg(u32_t argc, ...) {
  enum state {
    NONE, LEVEL, ENABLE, DISABLE
  } st = NONE;
  if (argc == 0) {
    print_debug_setting();
    return 0;
  }
  va_list va;
  va_start(va, argc);
  u32_t i;
  for (i = 0; i < argc; i++) {
    char *s = va_arg(va, char *);
    u32_t f = 0;
    if (!IS_STRING(s)) {
      return -1;
    }
    if (strcmp("level", s) == 0) {
      st = LEVEL;
    } else if (strcmp("on", s) == 0) {
      st = ENABLE;
    } else if (strcmp("off", s) == 0) {
      st = DISABLE;
    } else {
      switch (st) {
      case LEVEL:
        if (strcmp("dbg", s) == 0) {
          SYS_dbg_level(D_DEBUG);
        } else if (strcmp("info", s) == 0) {
          SYS_dbg_level(D_INFO);
        } else if (strcmp("warn", s) == 0) {
          SYS_dbg_level(D_WARN);
        } else if (strcmp("fatal", s) == 0) {
          SYS_dbg_level(D_FATAL);
        } else {
          return -1;
        }
        break;
      case ENABLE:
      case DISABLE: {
        int d;
        for (d = 0; f == 0 && d < sizeof(DBG_BIT_NAME) / sizeof(const char*);
            d++) {
          if (strcmp(DBG_BIT_NAME[d], s) == 0) {
            f = (1 << d);
          }
        }
        if (strcmp("all", s) == 0) {
          f = D_ANY;
        }
        if (f == 0) {
          return -1;
        }
        if (st == ENABLE) {
          SYS_dbg_mask_enable(f);
        } else {
          SYS_dbg_mask_disable(f);
        }
        break;
      }
      default:
        return -1;
      }
    }
  }
  print_debug_setting();
  va_end(va);
  return CLI_OK;
}
#endif

void __attribute__(( weak )) APP_dump(void) {
}

static int cli_dump(u32_t argc) {
  print("FULL DUMP\n=========\n");
  TASK_dump(IOSTD);
  print("\n");
#if CONFIG_OS && OS_DBG_MON
  OS_DBG_dump(IOSTD);
  print("\n");
#endif

  APP_dump();

  return CLI_OK;
}

static int cli_dump_trace(u32_t argc) {
#ifdef DBG_TRACE_MON
  SYS_dump_trace(IOSTD);
#else
  print("trace not enabled\n");
#endif
  return 0;
}

static int cli_memfind(u32_t argc, int hex) {
  if (argc != 1) return CLI_ERR_PARAM;
  u8_t *addr = (u8_t*)RAM_BEGIN;
  int i;
  print("finding 0x%08x in 0x%08x--0x%08x....\n", hex, RAM_BEGIN, RAM_END);
  for (i = 0; i < (u32_t)RAM_END - (u32_t)RAM_BEGIN - 4; i++) {
    u32_t m =
        (addr[i]) |
        (addr[i+1]<<8) |
        (addr[i+2]<<16) |
        (addr[i+3]<<24);
    u32_t rm =
        (addr[i+3]) |
        (addr[i+2]<<8) |
        (addr[i+1]<<16) |
        (addr[i]<<24);
    if (m == hex) {
      print("match found @ 0x%08x\n", i + addr);
    }
    if (rm == hex) {
      print("reverse match found @ 0x%08x\n", i + addr);
    }
  }
  print("finished\n");
  return CLI_OK;
}

static int cli_memrd(u32_t argc, u32_t address, u32_t len) {
  if (argc != 2) return CLI_ERR_PARAM;
  u8_t *addr = (u8_t*)address;
  u32_t i;
  for (i = 0; i < len; i++) {
    if ((i & 0xf) == 0) {
      print("\n0x%08x: ", address+i);
    }
    print("%02x ", addr[i]);
  }
  print("\n");
  return CLI_OK;
}

static int cli_memtest(u32_t argc, u32_t address, u32_t len) {
#ifdef EXTSRAM_ADDR
  if (argc == 0) {
    address = EXTSRAM_ADDR;
    len = EXTSRAM_LEN;
  } else
#endif
  if (argc != 2) return CLI_ERR_PARAM;
  u8_t *addr8 = (u8_t *)address;
  u16_t *addr16 = (u16_t *)address;
  u32_t *addr32 = (u32_t *)address;

  u32_t i;
  u16_t crc, crc_v;

  print("pseudo randomized memtest 0x%08x %d bytes\n", address, len);
  print("8 bit bus test\n");
  rand_seed(0x19760401);
  crc = 0xffff;
  for (i = 0; i < len; i++) {
    crc = crc_ccitt_16(crc, (u8_t)rand_next());
  }
  print("write\n");
  rand_seed(0x19760401);
  for (i = 0; i < len; i++) {
    addr8[i] = (u8_t)rand_next();
  }
  print("verify\n");
  rand_seed(0x19760401);
  crc_v = 0xffff;
  for (i = 0; i < len; i++) {
    u8_t ref = (u8_t)rand_next();
    u8_t val = addr8[i];
    crc_v = crc_ccitt_16(crc_v, val);
    if (val != ref) {
      print("ERROR  0x%08x differ, exp:%02x, got:%02x\n", &addr8[i], ref, val);
      goto end;
    }
  }
  if (crc != crc_v) {
    print("ERROR  CRC differ, exp:%04x, got:%04x\n", crc, crc_v);
    goto end;
  }
  print("16 bit bus test\n");
  len /= 2;
  rand_seed(0x19760401);
  crc = 0xffff;
  for (i = 0; i < len; i++) {
    u16_t v = (u16_t)rand_next();
    crc = crc_ccitt_16(crc, ((u8_t *)&v)[0]);
    crc = crc_ccitt_16(crc, ((u8_t *)&v)[1]);
  }
  print("write\n");
  rand_seed(0x19760401);
  for (i = 0; i < len; i++) {
    addr16[i] = (u16_t)rand_next();
  }
  print("verify\n");
  rand_seed(0x19760401);
  crc_v = 0xffff;
  for (i = 0; i < len; i++) {
    u16_t ref = (u16_t)rand_next();
    u16_t val = addr16[i];
    crc_v = crc_ccitt_16(crc_v, ((u8_t *)&val)[0]);
    crc_v = crc_ccitt_16(crc_v, ((u8_t *)&val)[1]);
    if (val != ref) {
      print("ERROR  0x%08x differ, exp:%04x, got:%04x\n", &addr16[i], ref, val);
      goto end;
    }
  }
  if (crc != crc_v) {
    print("ERROR  CRC differ, exp:%04x, got:%04x\n", crc, crc_v);
    goto end;
  }
  print("32 bit bus test\n");
  len /= 2;
  rand_seed(0x19760401);
  crc = 0xffff;
  for (i = 0; i < len; i++) {
    u32_t v = (u32_t)rand_next();
    crc = crc_ccitt_16(crc, ((u8_t *)&v)[0]);
    crc = crc_ccitt_16(crc, ((u8_t *)&v)[1]);
    crc = crc_ccitt_16(crc, ((u8_t *)&v)[2]);
    crc = crc_ccitt_16(crc, ((u8_t *)&v)[3]);
  }
  print("write\n");
  rand_seed(0x19760401);
  for (i = 0; i < len; i++) {
    addr32[i] = (u32_t)rand_next();
  }
  print("verify\n");
  rand_seed(0x19760401);
  crc_v = 0xffff;
  for (i = 0; i < len; i++) {
    u32_t ref = (u32_t)rand_next();
    u32_t val = addr32[i];
    crc_v = crc_ccitt_16(crc_v, ((u8_t *)&val)[0]);
    crc_v = crc_ccitt_16(crc_v, ((u8_t *)&val)[1]);
    crc_v = crc_ccitt_16(crc_v, ((u8_t *)&val)[2]);
    crc_v = crc_ccitt_16(crc_v, ((u8_t *)&val)[3]);
    if (val != ref) {
      print("ERROR  0x%08x differ, exp:%08x, got:%08x\n", &addr32[i], ref, val);
      goto end;
    }
  }
  if (crc != crc_v) {
    print("ERROR  CRC differ, exp:%04x, got:%04x\n", crc, crc_v);
    goto end;
  }

  print("OK\n");

  end:
  return CLI_OK;
}

static s32_t cli_reset(u32_t argc) {
  SYS_reboot(REBOOT_USER);
  return CLI_OK;
}

static s32_t cli_hardfault(u32_t argc) {
  u32_t *p = (u32_t *)0x00000001;
  *p = 123;
  return CLI_OK;
}

static s32_t cli_assert(u32_t argc) {
  ASSERT(FALSE);
  return CLI_OK;
}

CLI_MENU_START(system)
CLI_FUNC("assert", cli_assert, "Asserts")
CLI_FUNC("dbg", cli_dbg, "Set debug filter and level\n"
    "dbg (level <dbg|info|warn|fatal>) (on [x]*) (off [x]*)\n"
    "x - <sys|app|task|os|heap|comm|cli|nvs|spi|eth|fs|i2c|wifi|web|radio|all>\n"
    "ex: dbg level info off all on app sys\n")
CLI_FUNC("dump", cli_dump, "Dump system info")
CLI_FUNC("hardfault", cli_hardfault, "Hardfaults")
CLI_FUNC("memfind", cli_memfind, "Find 32 bit hex in memory\n"
    "memfind <value>")
CLI_FUNC("memrd", cli_memrd, "Read from memory\n"
    "memrd <addr> <len>")
CLI_FUNC("memtest", cli_memtest, "Run memory test\n"
    "memtest <addr> <len>")
CLI_FUNC("reset", cli_reset, "Resets processor")
CLI_FUNC("trace", cli_dump_trace, "Dump system trace")
CLI_MENU_END
