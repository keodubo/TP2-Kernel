#include <stdio.h>
#include <sys_calls.h>
#include <spawn_args.h>

// Convierte el código de estado numérico a su representación en texto
static const char *state_to_string(int state) {
	switch (state) {
	case 0:
		return "NEW";
	case 1:
		return "READY";
	case 2:
		return "RUN";
	case 3:
		return "BLOCK";
	case 4:
		return "EXIT";
	default:
		return "?";
	}
}

// Formatea un número de 64 bits como string hexadecimal
static void format_hex64(uint64_t value, char out[17]) {
	static const char *digits = "0123456789ABCDEF";
	for (int i = 15; i >= 0; i--) {
		out[i] = digits[value & 0xF];
		value >>= 4;
	}
	out[16] = '\0';
}

// Imprime un número de 64 bits en formato hexadecimal
static void print_hex64(uint64_t value) {
	char buf[17];
	format_hex64(value, buf);
	printf("0x%s", buf);
}

// Comando ps: Lista todos los procesos del sistema
// Muestra PID, prioridad, estado, ticks, FG/BG, stack pointer, base pointer y nombre
void ps_main(int argc, char **argv) {
	proc_info_t info[MAX_PROCS];
	int count = (int)sys_proc_snapshot(info, MAX_PROCS);

	if (count <= 0) {
		printf("\nNo processes to show\n");
		free_spawn_args(argv, argc);
		sys_exit(0);
	}

	// Imprimir encabezado de la tabla
	printf("\nPID   PRIO STATE  TICKS FG        SP                BP                NAME\n");

	// Imprimir información de cada proceso con alineación correcta
	for (int i = 0; i < count; i++) {
		const char *state = state_to_string(info[i].state);
		const char *fg = info[i].fg ? "FG" : "BG";
		
		// Print PID (width 5, left aligned)
		printf("%d", info[i].pid);
		int pid_len = (info[i].pid < 10) ? 1 : (info[i].pid < 100) ? 2 : (info[i].pid < 1000) ? 3 : (info[i].pid < 10000) ? 4 : 5;
		for (int j = pid_len; j < 6; j++) printf(" ");
		
		// Print Priority (width 4, left aligned)
		printf("%d", info[i].priority);
		int prio_len = (info[i].priority < 10) ? 1 : (info[i].priority < 100) ? 2 : (info[i].priority < 1000) ? 3 : 4;
		for (int j = prio_len; j < 5; j++) printf(" ");
		
		// Print State (width 6, left aligned)
		printf("%s", state);
		int state_len = 0;
		while (state[state_len]) state_len++;
		for (int j = state_len; j < 7; j++) printf(" ");
		
		// Print Ticks (width 5, left aligned)
		printf("%d", info[i].ticks_left);
		int ticks_len = (info[i].ticks_left < 10) ? 1 : (info[i].ticks_left < 100) ? 2 : (info[i].ticks_left < 1000) ? 3 : (info[i].ticks_left < 10000) ? 4 : 5;
		for (int j = ticks_len; j < 6; j++) printf(" ");
		
		// Print FG/BG (width 2, left aligned)
		printf("%s", fg);
		for (int j = 2; j < 10; j++) printf(" ");
		
		// Print SP and BP
		print_hex64(info[i].sp);
		printf(" ");
		print_hex64(info[i].bp);
		
		// Print Name
		printf(" %s\n", info[i].name[0] ? info[i].name : "(no name)");
	}
	printf("\n");  // Extra newline for readability

	free_spawn_args(argv, argc);
	sys_exit(0);
}
