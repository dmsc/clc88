#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emu.h"
#include "trace.h"
#include "utils.h"
#include "bus.h"
#include "cpu.h"
#include "cpu/m6502/m6502.h"
#include "frontend/frontend.h"
#include "monitor.h"

bool is_enabled = FALSE;
bool is_step    = FALSE;
bool is_stop_at_addr = FALSE;
unsigned stop_at_addr = 0;

v_cpu *cpu;

#define MAX_LINE_SIZE 1000

#define MAX_BREAKPOINTS 100
unsigned breakpoints[MAX_BREAKPOINTS];
unsigned breakpoints_count = 0;

void monitor_init(v_cpu *monitor_cpu) {
	cpu = monitor_cpu;
}

void monitor_enable() {
	is_enabled = TRUE;
}

void monitor_disable() {
	is_enabled = FALSE;
}

bool monitor_is_enabled() {
	return is_enabled;
}

static void dump_registers() {
	char register_info[1000];
	if (cpu->cpuType == CPU_M6502) {
		sprintf(register_info,
			"A:%02X X:%02X Y:%02X P:%02X S:%02X",
			cpu->get_reg(M6502_A),
			cpu->get_reg(M6502_X),
			cpu->get_reg(M6502_Y),
			cpu->get_reg(M6502_P),
			cpu->get_reg(M6502_S)
		);
	}
	printf("%s\n", register_info);
}

static unsigned dump_code(unsigned addr) {
	char disasm[100];
	unsigned next_addr = cpu->disasm(addr, disasm);

	char multi_byte[20]  = "";
	char single_byte[20] = "";
	int instructions = next_addr - addr;
	for(int i=0; i<3; i++) {
		if (i<instructions) {
			sprintf(single_byte, "%02X ", bus_read16(addr+i));
			strcat(multi_byte, single_byte);
		} else {
			strcat(multi_byte, "   ");
		}
	}

	char code[100];
	sprintf(code, "%04X %s %s  %s", addr,
			monitor_is_breakpoint(addr) ? "*":" ",
			multi_byte, utils_str2upper(disasm));
	printf("%s\n", code);
	return next_addr;
}

static unsigned disasm(unsigned addr, int lines) {
	for(int i=0; i<lines; i++) {
		addr = dump_code(addr);
	}
	return addr;
}

static unsigned parse_hex(char *s) {
	char *saddr = utils_trim(s);

	unsigned addr;
	sscanf(saddr, "%x", &addr);
	return addr;
}

static void breakpoint_set(unsigned addr) {
	if (breakpoints_count == MAX_BREAKPOINTS) return;
	// ignore if breakpoint exists
	for(int i=0; i<breakpoints_count; i++) {
		if (breakpoints[i] == addr) return;
	}
	// add breakpoint
	breakpoints[breakpoints_count++] = addr;
}

static void breakpoint_del(unsigned index) {
	if (index >= breakpoints_count) return;

	// remove breakpoint
	for(int i = index; i<breakpoints_count-1; i++) {
		breakpoints[i] = breakpoints[i+1];
	}
	breakpoints_count--;
}

void breakpoints_list() {
	for(int i=0; i<breakpoints_count; i++) {
		printf("%02d: ", i);
		dump_code(breakpoints[i]);
	}
}

bool monitor_is_stop(unsigned addr) {
	return monitor_is_breakpoint(addr)
			|| is_step
			|| (is_stop_at_addr && addr == stop_at_addr);
}

bool monitor_is_breakpoint(unsigned addr) {
	for(int i=0; i<breakpoints_count; i++) {
		if (breakpoints[i] == addr) return TRUE;
	}

	return FALSE;
}

void monitor_help() {
	printf("\nCompy monitor\n\n");
	printf("Commands:\n");
	printf("r             Display Registers\n");
	printf("d             Disassembly\n");
	printf("d addr        Disassembly from address\n");
	printf("da            Disassembly (again) from PC address\n");
	printf("s             Step one instruction\n");
	printf("g             Run\n");
	printf("g addr        Run up to the specified address\n");
	printf("b             Display breakpoints\n");
	printf("b [set] addr  Set breakpoints at addr\n");
	printf("b del pos     Del breakpoint at position\n");
	printf("h             This help\n");
	printf("x             Exit emulator\n\n");
}

void monitor_enter() {
	if (!frontend_running()) return;
	is_step = FALSE;
	is_enabled = FALSE;

	bool trace_was_enabled = trace_enabled;
	trace_enabled = FALSE;

	frontend_process_events_async_start();

	unsigned dasm_start = cpu->get_pc();

	dump_registers();
	dump_code(cpu->get_pc());

	char buffer[MAX_LINE_SIZE+1];

	unsigned nparts = 0;
	bool in_loop = TRUE;
	while(in_loop && frontend_running()) {
		printf(">");
		fgets(buffer, MAX_LINE_SIZE, stdin);
		char *line = strdup(utils_trim(buffer));

		char **parts = utils_split(line, &nparts);

		if (nparts == 0 || !strcmp(parts[0],"r")) {
			dump_registers();
			dump_code(cpu->get_pc());
			continue;
		} else if (!strcmp(parts[0], "d")) {
			if (nparts == 1) {
				dasm_start = disasm(dasm_start, 16);
			} else {
				unsigned addr = parse_hex(parts[1]);
				dasm_start = disasm(addr, 16);
			}
		} else if (!strcmp(parts[0], "da")) {
			dasm_start = disasm(cpu->get_pc(), 16);
		} else if (!strcmp(parts[0], "g")) {
			in_loop = FALSE;
			if (nparts >1) {
				unsigned addr = parse_hex(parts[1]);
				stop_at_addr = addr;
				is_stop_at_addr = TRUE;
			}
		} else if (!strcmp(parts[0], "b")) {
			if (nparts > 2) {
				unsigned addr = parse_hex(parts[2]);
				if (!strcmp(parts[1], "set")) {
					breakpoint_set(addr);
				} else if (!strcmp(parts[1], "del")) {
					breakpoint_del(addr);
				}
			} else if (nparts > 1) {
				unsigned addr = parse_hex(parts[1]);
				breakpoint_set(addr);
			}
			breakpoints_list();
		} else if (!strcmp(parts[0], "s")) {
			is_step = TRUE;
			in_loop = FALSE;
		} else if (!strcmp(parts[0], "h")) {
			monitor_help();
		} else if (!strcmp(parts[0], "x")) {
			in_loop = FALSE;
			frontend_shutdown();
		}
		free(line);
	}
	frontend_process_events_async_stop();
	trace_enabled = trace_was_enabled;
}

