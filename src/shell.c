#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>      // Pour open(), O_RDONLY, etc.
#include "shell.h"

#define HISTORY_FILE ".vortish_history"

// Variables globales pour l'historique
char *history[MAX_HISTORY];
int history_count = 0;

// ==================== HISTORIQUE ====================

void add_to_history(const char *command) {
    if (strlen(command) == 0) return;
    
    if (history_count > 0 && strcmp(history[history_count - 1], command) == 0) {
        return;
    }
    
    if (history_count >= MAX_HISTORY) {
        free(history[0]);
        for (int i = 1; i < MAX_HISTORY; i++) {
            history[i - 1] = history[i];
        }
        history_count--;
    }
    
    history[history_count] = malloc(strlen(command) + 1);
    strcpy(history[history_count], command);
    history_count++;
}

void show_history() {
    printf("\n\033[1;36mHistorique des commandes :\033[0m\n");
    for (int i = 0; i < history_count; i++) {
        printf("  \033[1;33m%3d\033[0m  %s\n", i + 1, history[i]);
    }
    printf("\n");
}

void save_history_to_file() {
    FILE *file = fopen(HISTORY_FILE, "w");
    if (file == NULL) return;
    
    for (int i = 0; i < history_count; i++) {
        fprintf(file, "%s\n", history[i]);
    }
    fclose(file);
}

void load_history_from_file() {
    FILE *file = fopen(HISTORY_FILE, "r");
    if (file == NULL) return;
    
    char buffer[MAX_COMMAND_LENGTH];
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        buffer[strcspn(buffer, "\n")] = 0;
        if (history_count < MAX_HISTORY) {
            history[history_count] = malloc(strlen(buffer) + 1);
            strcpy(history[history_count], buffer);
            history_count++;
        }
    }
    fclose(file);
}

void execute_history_command(int num) {
    if (num < 1 || num > history_count) {
        printf("\033[1;31mErreur : numéro d'historique invalide\033[0m\n");
        return;
    }
    char *cmd = history[num - 1];
    printf("Exécution : %s\n", cmd);
    execute_command(cmd);
}

// ==================== PIPES ====================

int count_pipes(char *command) {
    int count = 0;
    for (int i = 0; command[i] != '\0'; i++) {
        if (command[i] == '|') {
            count++;
        }
    }
    return count;
}

void parse_pipes(char *command, char **pipe_commands) {
    int i = 0;
    pipe_commands[i] = strtok(command, "|");
    while (pipe_commands[i] != NULL && i < MAX_PIPES - 1) {
        i++;
        pipe_commands[i] = strtok(NULL, "|");
    }
    pipe_commands[i] = NULL;
}

void execute_pipe_chain(char **commands, int count) {
    int i;
    int pipefd[2];
    int input_fd = STDIN_FILENO;
    pid_t pid;
    
    for (i = 0; i < count; i++) {
        if (i < count - 1) {
            if (pipe(pipefd) == -1) {
                perror("pipe");
                return;
            }
        }
        
        pid = fork();
        if (pid == 0) {
            if (input_fd != STDIN_FILENO) {
                dup2(input_fd, STDIN_FILENO);
                close(input_fd);
            }
            
            if (i < count - 1) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }
            
            if (i < count - 1) {
                close(pipefd[0]);
            }
            
            char *args[MAX_ARGS];
            char *token;
            int j = 0;
            char cmd_copy[MAX_COMMAND_LENGTH];
            
            strcpy(cmd_copy, commands[i]);
            token = strtok(cmd_copy, " \t");
            while (token != NULL && j < MAX_ARGS - 1) {
                while (*token == ' ' || *token == '\t') token++;
                if (strlen(token) > 0) {
                    args[j] = token;
                    j++;
                }
                token = strtok(NULL, " \t");
            }
            args[j] = NULL;
            
            execvp(args[0], args);
            printf("\033[1;31mCommande non trouvée : %s\033[0m\n", args[0]);
            exit(1);
        }
        else if (pid > 0) {
            if (input_fd != STDIN_FILENO) {
                close(input_fd);
            }
            
            if (i < count - 1) {
                close(pipefd[1]);
                input_fd = pipefd[0];
            }
        }
        else {
            printf("\033[1;31mErreur lors du fork\033[0m\n");
            return;
        }
    }
    
    for (i = 0; i < count; i++) {
        wait(NULL);
    }
}

// ==================== REDIRECTIONS ====================

// Vérifie si la commande contient des redirections
int has_redirections(char *command) {
    for (int i = 0; command[i] != '\0'; i++) {
        if (command[i] == '>' || command[i] == '<') {
            return 1;
        }
    }
    return 0;
}

// Extrait les fichiers et arguments des redirections
void parse_redirections(char *command, char **args, 
                        char **input_file, char **output_file, int *append) {
    *input_file = NULL;
    *output_file = NULL;
    *append = 0;
    
    char *token;
    char copy[MAX_COMMAND_LENGTH];
    strcpy(copy, command);
    
    int i = 0;
    token = strtok(copy, " \t");
    
    while (token != NULL && i < MAX_ARGS - 1) {
        if (strcmp(token, "<") == 0) {
            // Redirection d'entrée
            token = strtok(NULL, " \t");
            if (token != NULL) {
                *input_file = strdup(token);
            }
        }
        else if (strcmp(token, ">") == 0) {
            // Redirection de sortie (écrasement)
            token = strtok(NULL, " \t");
            if (token != NULL) {
                *output_file = strdup(token);
                *append = 0;
            }
        }
        else if (strcmp(token, ">>") == 0) {
            // Redirection de sortie (ajout)
            token = strtok(NULL, " \t");
            if (token != NULL) {
                *output_file = strdup(token);
                *append = 1;
            }
        }
        else {
            // Argument normal de la commande
            args[i] = token;
            i++;
        }
        token = strtok(NULL, " \t");
    }
    args[i] = NULL;
}

// Exécute une commande avec les redirections
int handle_redirections(char **args, char *input_file, 
                        char *output_file, int append) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Gérer la redirection d'entrée (<)
        if (input_file != NULL) {
            int fd = open(input_file, O_RDONLY);
            if (fd == -1) {
                printf("\033[1;31mErreur: Impossible d'ouvrir %s\033[0m\n", input_file);
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        
        // Gérer la redirection de sortie (> ou >>)
        if (output_file != NULL) {
            int flags = O_WRONLY | O_CREAT;
            if (append) {
                flags |= O_APPEND;
            } else {
                flags |= O_TRUNC;
            }
            int fd = open(output_file, flags, 0644);
            if (fd == -1) {
                printf("\033[1;31mErreur: Impossible d'écrire dans %s\033[0m\n", output_file);
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        
        // Exécuter la commande
        execvp(args[0], args);
        printf("\033[1;31mCommande non trouvée : %s\033[0m\n", args[0]);
        exit(1);
    }
    else if (pid > 0) {
        int status;
        wait(&status);
        return status;
    }
    else {
        printf("\033[1;31mErreur lors du fork\033[0m\n");
        return -1;
    }
}

// ==================== COMMANDES INTERNES ====================

void show_help() {
    printf("\n\033[1;32mCommandes disponibles :\033[0m\n");
    printf("  \033[1;36mhelp\033[0m     - Afficher cette aide\n");
    printf("  \033[1;36mhistory\033[0m  - Afficher l'historique\n");
    printf("  \033[1;36m!N\033[0m        - Exécuter commande N de l'historique\n");
    printf("  \033[1;36m!!\033[0m        - Exécuter la dernière commande\n");
    printf("  \033[1;36mexit\033[0m     - Quitter\n");
    printf("  \033[1;36mclear\033[0m    - Effacer l'écran\n");
    printf("  \033[1;36mvortish\033[0m  - Afficher la bannière\n");
    
    printf("\n\033[1;33mPipes :\033[0m\n");
    printf("  \033[1;36mcommande1 | commande2\033[0m\n");
    printf("  Exemple: \033[1;36mls -la | grep .c | wc -l\033[0m\n");
    
    printf("\n\033[1;33mRedirections :\033[0m\n");
    printf("  \033[1;36mcommande > fichier\033[0m   - Écrire dans un fichier\n");
    printf("  \033[1;36mcommande >> fichier\033[0m  - Ajouter à un fichier\n");
    printf("  \033[1;36mcommande < fichier\033[0m   - Lire depuis un fichier\n");
    printf("  Exemple: \033[1;36mls -la > liste.txt\033[0m\n");
    printf("  Exemple: \033[1;36mwc -l < liste.txt\033[0m\n\n");
}

// ==================== EXÉCUTION PRINCIPALE ====================

void execute_command(char *command) {
    // Ignorer les commandes vides
    if (strlen(command) == 0) return;
    
    // Ajouter à l'historique (sauf commandes spéciales)
    if (strcmp(command, "history") != 0 && 
        strncmp(command, "!", 1) != 0) {
        add_to_history(command);
    }
    
    // Commandes internes (avant pipes et redirections)
    if (strcmp(command, "exit") == 0) {
        save_history_to_file();
        printf("Au revoir ! 👋\n");
        exit(0);
    }
    else if (strcmp(command, "help") == 0) {
        show_help();
        return;
    }
    else if (strcmp(command, "history") == 0) {
        show_history();
        return;
    }
    else if (strcmp(command, "!!") == 0) {
        if (history_count > 0) {
            execute_history_command(history_count);
        } else {
            printf("\033[1;31mHistorique vide\033[0m\n");
        }
        return;
    }
    else if (command[0] == '!') {
        int num = atoi(command + 1);
        if (num > 0) {
            execute_history_command(num);
        } else {
            printf("\033[1;31mFormat invalide. Utilisez !N (ex: !5)\033[0m\n");
        }
        return;
    }
    else if (strcmp(command, "clear") == 0) {
        printf("\033[2J\033[1;1H");
        return;
    }
    else if (strcmp(command, "vortish") == 0) {
        printf("\n\033[1;35m╔══════════════════════════════════╗\n");
        printf("║        VORTISH SHELL v1.3       ║\n");
        printf("║   (pipes + historique + redirections) ║\n");
        printf("╚══════════════════════════════════╝\033[0m\n\n");
        return;
    }
    
    // Vérifier les pipes d'abord
    int pipe_count = count_pipes(command);
    if (pipe_count > 0) {
        char *pipe_commands[MAX_PIPES];
        char command_copy[MAX_COMMAND_LENGTH];
        
        strcpy(command_copy, command);
        parse_pipes(command_copy, pipe_commands);
        
        int cmd_count = 0;
        while (pipe_commands[cmd_count] != NULL && cmd_count < MAX_PIPES) {
            cmd_count++;
        }
        
        if (cmd_count >= 2) {
            execute_pipe_chain(pipe_commands, cmd_count);
            return;
        }
    }
    
    // Vérifier les redirections
    if (has_redirections(command)) {
        char *args[MAX_ARGS];
        char *input_file = NULL;
        char *output_file = NULL;
        int append = 0;
        char command_copy[MAX_COMMAND_LENGTH];
        
        strcpy(command_copy, command);
        parse_redirections(command_copy, args, &input_file, &output_file, &append);
        
        if (args[0] != NULL) {
            handle_redirections(args, input_file, output_file, append);
        }
        
        // Libérer la mémoire allouée
        if (input_file) free(input_file);
        if (output_file) free(output_file);
        return;
    }
    
    // Commande simple (sans pipe ni redirection)
    pid_t pid = fork();
    
    if (pid == 0) {
        char *args[MAX_ARGS];
        char *token;
        int i = 0;
        char command_copy[MAX_COMMAND_LENGTH];
        
        strcpy(command_copy, command);
        token = strtok(command_copy, " ");
        while (token != NULL && i < MAX_ARGS - 1) {
            args[i] = token;
            token = strtok(NULL, " ");
            i++;
        }
        args[i] = NULL;
        
        execvp(args[0], args);
        printf("\033[1;31mCommande non trouvée : %s\033[0m\n", args[0]);
        exit(1);
    }
    else if (pid > 0) {
        wait(NULL);
    }
    else {
        printf("\033[1;31mErreur lors de l'exécution\033[0m\n");
    }
}

void run_shell() {
    char command[MAX_COMMAND_LENGTH];
    load_history_from_file();
    
    while (1) {
        printf("\033[1;32mvortish@ubuntu\033[0m:\033[1;34m~$\033[0m ");
        
        if (fgets(command, MAX_COMMAND_LENGTH, stdin) == NULL) {
            break;
        }
        
        command[strcspn(command, "\n")] = 0;
        execute_command(command);
    }
}