#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "shell.h"

#define HISTORY_FILE ".vortish_history"

// Variables globales pour l'historique
char *history[MAX_HISTORY];
int history_count = 0;

// Ajouter une commande à l'historique
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

// Compter le nombre de pipes dans la commande
int count_pipes(char *command) {
    int count = 0;
    for (int i = 0; command[i] != '\0'; i++) {
        if (command[i] == '|') {
            count++;
        }
    }
    return count;
}

// Découper la commande en segments séparés par des pipes
void parse_pipes(char *command, char **pipe_commands) {
    int i = 0;
    pipe_commands[i] = strtok(command, "|");
    while (pipe_commands[i] != NULL && i < MAX_PIPES - 1) {
        i++;
        pipe_commands[i] = strtok(NULL, "|");
    }
    pipe_commands[i] = NULL;
}

// Exécuter une chaîne de pipes
void execute_pipe_chain(char **commands, int count) {
    int i;
    int pipefd[2];
    int input_fd = STDIN_FILENO;  // Descripteur d'entrée pour la première commande
    pid_t pid;
    
    for (i = 0; i < count; i++) {
        // Créer un pipe pour la communication (sauf pour la dernière commande)
        if (i < count - 1) {
            if (pipe(pipefd) == -1) {
                perror("pipe");
                return;
            }
        }
        
        pid = fork();
        if (pid == 0) {
            // Processus enfant
            
            // Rediriger l'entrée depuis le pipe précédent
            if (input_fd != STDIN_FILENO) {
                dup2(input_fd, STDIN_FILENO);
                close(input_fd);
            }
            
            // Rediriger la sortie vers le pipe suivant (sauf pour la dernière)
            if (i < count - 1) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }
            
            // Fermer les descripteurs inutilisés
            if (i < count - 1) {
                close(pipefd[0]);
            }
            
            // Préparer les arguments pour la commande
            char *args[MAX_ARGS];
            char *token;
            int j = 0;
            char cmd_copy[MAX_COMMAND_LENGTH];
            
            strcpy(cmd_copy, commands[i]);
            token = strtok(cmd_copy, " \t");
            while (token != NULL && j < MAX_ARGS - 1) {
                // Enlever les espaces en début et fin
                while (*token == ' ' || *token == '\t') token++;
                if (strlen(token) > 0) {
                    args[j] = token;
                    j++;
                }
                token = strtok(NULL, " \t");
            }
            args[j] = NULL;
            
            // Exécuter la commande
            execvp(args[0], args);
            
            // Si on arrive ici, c'est que execvp a échoué
            printf("\033[1;31mCommande non trouvée : %s\033[0m\n", args[0]);
            exit(1);
        }
        else if (pid > 0) {
            // Processus parent
            
            // Attendre que l'enfant termine (optionnel - on peut attendre à la fin)
            // wait(NULL);
            
            // Fermer les descripteurs du parent
            if (input_fd != STDIN_FILENO) {
                close(input_fd);
            }
            
            if (i < count - 1) {
                close(pipefd[1]);  // Fermer l'écriture du parent
                input_fd = pipefd[0];  // Lire depuis ce pipe pour la prochaine commande
            }
        }
        else {
            // Erreur de fork
            printf("\033[1;31mErreur lors du fork\033[0m\n");
            return;
        }
    }
    
    // Attendre que tous les enfants terminent
    for (i = 0; i < count; i++) {
        wait(NULL);
    }
}

// Exécuter la commande historique par numéro
void execute_history_command(int num) {
    if (num < 1 || num > history_count) {
        printf("\033[1;31mErreur : numéro d'historique invalide\033[0m\n");
        return;
    }
    char *cmd = history[num - 1];
    printf("Exécution : %s\n", cmd);
    execute_command(cmd);
}

// Fonction principale d'exécution des commandes
void execute_command(char *command) {
    // Ignorer les commandes vides
    if (strlen(command) == 0) return;
    
    // Ajouter à l'historique
    if (strcmp(command, "history") != 0 && 
        strncmp(command, "!", 1) != 0) {
        add_to_history(command);
    }
    
    // Vérifier s'il y a des pipes
    int pipe_count = count_pipes(command);
    
    if (pipe_count > 0) {
        // Gestion des commandes avec pipes
        char *pipe_commands[MAX_PIPES];
        char command_copy[MAX_COMMAND_LENGTH];
        
        strcpy(command_copy, command);
        parse_pipes(command_copy, pipe_commands);
        
        // Compter le nombre réel de commandes
        int cmd_count = 0;
        while (pipe_commands[cmd_count] != NULL && cmd_count < MAX_PIPES) {
            cmd_count++;
        }
        
        if (cmd_count >= 2) {
            execute_pipe_chain(pipe_commands, cmd_count);
            return;
        }
    }
    
    // Gestion des commandes internes
    if (strcmp(command, "exit") == 0) {
        save_history_to_file();
        printf("Au revoir ! 👋\n");
        exit(0);
    }
    else if (strcmp(command, "help") == 0) {
        printf("\n\033[1;32mCommandes disponibles :\033[0m\n");
        printf("  \033[1;36mhelp\033[0m     - Afficher cette aide\n");
        printf("  \033[1;36mhistory\033[0m  - Afficher l'historique\n");
        printf("  \033[1;36m!N\033[0m        - Exécuter commande N de l'historique\n");
        printf("  \033[1;36m!!\033[0m        - Exécuter la dernière commande\n");
        printf("  \033[1;36mexit\033[0m     - Quitter\n");
        printf("  \033[1;36mclear\033[0m    - Effacer l'écran\n");
        printf("  \033[1;36mvortish\033[0m  - Afficher la bannière\n");
        printf("\n\033[1;33mNouveau ! Support des pipes :\033[0m\n");
        printf("  \033[1;36mcommande1 | commande2\033[0m\n");
        printf("  Exemple: \033[1;36mls -la | grep .c | wc -l\033[0m\n\n");
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
        printf("║        VORTISH SHELL v1.2       ║\n");
        printf("║    (avec pipes et historique)   ║\n");
        printf("╚══════════════════════════════════╝\033[0m\n\n");
        return;
    }
    
    // Commande simple (sans pipe)
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