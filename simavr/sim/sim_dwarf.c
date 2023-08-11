
/*
	sim_dwarf.c

	Extract debug symbols and line numbers from  .elf file for use
        by gdb ("info io_registers" command) and tracing.

	Copyright 2021, 2022 Giles Atkinson

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_LIBDWARF

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <setjmp.h>
#include <dwarf.h>
#include <libdwarf/libdwarf.h>

#include "sim_avr.h"

//#define VERBOSE
#define CHECK(fn) \
    if (rv == DW_DLV_ERROR) { error(fn, err); siglongjmp(ctxp->err_jmp, 1); }
#define DATA_OFFSET 0x800000

/* Context passed to functions in this file. */

struct ctx {
    avr_t              *avr;
    Dwarf_Debug         db;             // libdwarf instance handle.

    // Source lines.

    Dwarf_Line_Context  lc;
    Dwarf_Line         *lines;
    Dwarf_Signed        line_count;
    char               *cu_name;        // Name of current Compilation Unit.

    // Error handling.

    sigjmp_buf          err_jmp;
};

static void error(char *fn, Dwarf_Error err)
{
    fprintf(stderr, "%s() failed: %s\n", fn, dwarf_errmsg(err));
}

#if CONFIG_SIMAVR_TRACE
static void set_flash_name(avr_t *avr, Dwarf_Addr symv, const char *name)
{
    const char **ep;

    symv >>= 1;
    if (symv >= avr->trace_data->codeline_size)
        return;
    ep = avr->trace_data->codeline + symv;
    if (!*ep)
        *ep = strdup(name);
}
#endif

static void process(struct ctx *ctxp, Dwarf_Die die)
{
    char          *name = 0;
    Dwarf_Addr     addr, addr2;
    Dwarf_Half     tag;
    Dwarf_Error    err;
    int            rv;

    rv = dwarf_diename(die, &name, &err);
    if (rv == DW_DLV_NO_ENTRY)
        return;
    CHECK("dwarf_diename");
    rv = dwarf_tag(die, &tag, &err);
    CHECK("dwarf_tag");
    if (tag == DW_TAG_variable) {
        Dwarf_Attribute  attr;
        Dwarf_Unsigned   count, op_count, symv, op2, op3, off1, off2;
        Dwarf_Loc_Head_c head;
        Dwarf_Locdesc_c  loc;
        Dwarf_Small      op, value, list_type;

        /* Get the location (address) information for this variable.
         * Only static allocations are of interest.
         */

        rv = dwarf_attr(die, DW_AT_location, &attr, &err);
        CHECK("dwarf_attr");
        if (rv ==  DW_DLV_NO_ENTRY)
            goto clean;

        rv = dwarf_get_loclist_c(attr, &head, &count, &err);
        CHECK("dwarf_get_loclist_c");
        if (rv ==  DW_DLV_NO_ENTRY)
            goto clean;
        rv = dwarf_get_locdesc_entry_c(head, 0, &value, &addr, &addr2,
                                       &op_count, &loc, &list_type,
                                       &off1, &off2, &err);
        CHECK("dwarf_get_locdesc_entry_c");

        if (list_type == 0 && count == 1) {
            /* Probably statically-allocated. */

            rv = dwarf_get_location_op_value_c(loc, 0, &op, &symv, &op2, &op3,
                                               &off1, &err);
            CHECK("dwarf_get_location_op_value_c");

            if (op == DW_OP_addr) {
                avr_t *avr;

                /* Static variable. */

                avr = ctxp->avr;
#ifdef VERBOSE
                printf("%s = %llx\n", name, symv);
#endif
                if (symv > DATA_OFFSET) {
                    symv -= DATA_OFFSET;


                    /* Is it an I/O register or RAM? */

                    if (symv > 32 &&
#if CONFIG_SIMAVR_TRACE
                    symv < avr->trace_data->data_names_size &&
#else
                    symv <= avr->ioend &&
#endif
                        !avr->data_names[symv]) {
                        avr->data_names[symv] = strdup(name);
                    }
                }
#if CONFIG_SIMAVR_TRACE
                else {
                    /* Data address in flash. */

                    set_flash_name(avr, symv, name);
                }
#endif
            }
        }
        dwarf_loc_head_c_dealloc(head);
#if CONFIG_SIMAVR_TRACE
    } else if (tag == DW_TAG_subprogram) {
        rv = dwarf_lowpc(die, &addr, &err);
        if (rv == DW_DLV_NO_ENTRY) {
            /* In AVR code these seem to be inlined functions, but why no
             * addresses?
             */

            goto clean;
        }
        CHECK("dwarf_lowpc");
        set_flash_name(ctxp->avr, addr, name);

#ifdef VERBOSE
        enum Dwarf_Form_Class class;
        
        rv = dwarf_highpc_b(die, &addr2, &tag, &class, &err);
        if (rv == DW_DLV_NO_ENTRY)
            goto clean;
        CHECK("dwarf_highpc_b");
        if (tag == DW_FORM_CLASS_CONSTANT) {  // Offset from low.
            addr2 += addr;
        } else if (tag != DW_FORM_CLASS_ADDRESS) {
            /* LOCLISTPTR shows up on Linux X64.  Give up. */

            addr2 = 0;
        }
        printf("%s: %#llx - %#llx\n", name, addr, addr2);
#endif  // VERBOSE
#endif
    }
 clean:
    dwarf_dealloc(ctxp->db, name, DW_DLA_STRING);
}

static void traverse_tree(struct ctx *ctxp, Dwarf_Die start)
{
    Dwarf_Debug    db;
    Dwarf_Die      die;
    Dwarf_Error    err;
    int            rv;

    process(ctxp, start);
    rv = dwarf_child(start, &die, &err);
    if (rv == DW_DLV_OK)
        traverse_tree(ctxp, die);
    else 
        CHECK("dwarf_child");

    /* Examine siblings. */

    db = ctxp->db;
    rv = dwarf_siblingof_b(db, start, 1, &die, &err);
    if (rv == DW_DLV_OK)
        traverse_tree(ctxp, die);
    else 
        CHECK("dwarf_siblingof_b");
    dwarf_dealloc(db, start, DW_DLA_DIE);
}

#if CONFIG_SIMAVR_TRACE
static void get_lines(struct ctx *ctxp, Dwarf_Die die)
{
    Dwarf_Unsigned      version;
    Dwarf_Error         err;
    Dwarf_Small         single;
    int                 rv;

    rv = dwarf_srclines_b(die, &version, &single, &ctxp->lc, &err);
    CHECK("dwarf_srclines_b");
    rv = dwarf_srclines_from_linecontext(ctxp->lc, &ctxp->lines,
                                         &ctxp->line_count, &err);
    CHECK("dwarf_srclines_from_linecontext");
    if (ctxp->line_count == 0)
        return;

    rv = dwarf_diename(die, &ctxp->cu_name, &err);
    if (rv == DW_DLV_NO_ENTRY)
        ctxp->cu_name = "";
    else
        CHECK("dwarf_diename");
#ifdef VERBOSE
    printf("Line table for %s:\n\n", ctxp->cu_name);
    for (int i = 0; i < ctxp->line_count; ++i) {
        Dwarf_Unsigned    lineno;
        Dwarf_Addr        addr;

        rv = dwarf_lineno(ctxp->lines[i], &lineno, &err);
        CHECK("dwarf_lineno");
        rv = dwarf_lineaddr(ctxp->lines[i], &addr, &err);
        CHECK("dwarf_lineaddr");

        printf("%lld %llx\n", lineno, addr);
    }
    printf("\n");
#endif
}
#endif

int avr_read_dwarf(avr_t *avr, const char *filename)
{
    struct ctx      ctx, *ctxp;
    Dwarf_Die       die;
    Dwarf_Unsigned  next = 0;
    Dwarf_Half      type;
    Dwarf_Error     err;
    int             rv, fd;

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror(filename);
        return 1;
    }

    ctx.avr = avr;
    rv = dwarf_init_b(fd, DW_DLC_READ, 0, NULL, NULL, &ctx.db, &err);
    if (rv != DW_DLV_OK) {
        error("dwarf_init_b", err);
        return 1;
    }

    if (sigsetjmp(ctx.err_jmp, 0)) {
        dwarf_finish(ctx.db, &err);
        close(fd);
        return -1;
    }
    ctxp = &ctx;                // For CHECK macro

    /* Loop over all Compilation Units. */

    for(;;) {
        rv = dwarf_next_cu_header_d(ctx.db, 1, NULL, NULL, NULL, NULL, NULL,
                                    NULL, NULL, NULL, &next, &type, &err);
        if (rv != DW_DLV_OK) {
            if (rv == DW_DLV_NO_ENTRY)
                break;
            error("dwarf__next_cu_header_d", err);
        }

        /* Sibling of NULL is Compiliation Unit's Debug Information Element. */

        rv = dwarf_siblingof_b(ctx.db, NULL, 1, &die, &err);
        CHECK("dwarf_siblingof_b");

#if CONFIG_SIMAVR_TRACE
        const char *last_symbol = NULL;
        int         i, prev_line;


        get_lines(&ctx, die);
        traverse_tree(&ctx, die);
        for (i = 0, prev_line = -1; i < ctx.line_count; ++i) {
            Dwarf_Unsigned    lineno;
            Dwarf_Addr        addr;
            const char      **ep;
            char              buff[128];

            rv = dwarf_lineno(ctx.lines[i], &lineno, &err);
            CHECK("dwarf_lineno");
            rv = dwarf_lineaddr(ctx.lines[i], &addr, &err);
            CHECK("dwarf_lineaddr");
            if (prev_line == lineno)
                continue;	// Ignore duplicates
            prev_line = lineno;
            if (addr == 0) // Inlined?
                continue;
            ep = avr->trace_data->codeline + (addr >> 1);
            if (*ep) {
                // Already labeled.

                last_symbol = *ep;
                continue;
            } else if (last_symbol) {
                snprintf(buff, sizeof buff, "%s L.%lld", last_symbol, lineno);
            } else {
                snprintf(buff, sizeof buff, "%s, line %lld",
                         ctx.cu_name, lineno);
            }
            *ep = strdup(buff);
        }
        dwarf_dealloc(ctx.db, ctx.cu_name, DW_DLA_STRING);
        dwarf_srclines_dealloc_b(ctx.lc);
#else
        traverse_tree(&ctx, die);
#endif
    }
    dwarf_finish(ctx.db, &err);
    if (rv == DW_DLV_ERROR)
        error("dwarf_finish", err);
    close(fd);
    return 0;
}

# else // No libdwarf
#include "sim_avr.h"
int avr_read_dwarf(avr_t *avr, const char *filename) { return 0; }
#endif
