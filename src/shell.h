#ifndef SHELL_H
#define SHELL_H

#define MAX_HISTORY 100
#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64
#define MAX_PIPES 10  // Nombre maximum de pipes dans une commande

void run_shell();
void execute_command(char *command);
void add_to_history(const char *command);
void show_history();
void save_history_to_file();
void load_history_from_file();

// Nouveaux prototypes pour les pipes
int count_pipes(char *command);
void execute_pipe_chain(char **commands, int count);
void parse_pipes(char *command, char **pipe_commands);

#endif