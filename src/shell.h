#ifndef SHELL_H
#define SHELL_H

#define MAX_HISTORY 100 // taille maximale de l'historique


void run_shell();
void execute_command(char *command);
void add_to_history(const char *command);
void show_history();
void save_history_to_file();
void load_history_from_file();

#endif