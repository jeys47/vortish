#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "shell.h"

#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64
#define HISTORY_FILE ".vortish_history"

// Variables globales pour l'historique
char *history[MAX_HISTORY];
int history_count = 0;
int history_current = 0;  // Pour la navigation (sera utilisé plus tard)

// Ajouter une commande à l'historique
void add_to_history(const char *command) {
    // Ne pas ajouter les commandes vides
    if (strlen(command) == 0) {
        return;
    }
    
    // Ne pas ajouter si c'est la même que la dernière commande
    if (history_count > 0 && strcmp(history[history_count - 1], command) == 0) {
        return;
    }
    
    // Si l'historique est plein, libérer la plus ancienne
    if (history_count >= MAX_HISTORY) {
        free(history[0]);
        // Décaler toutes les entrées vers la gauche
        for (int i = 1; i < MAX_HISTORY; i++) {
            history[i - 1] = history[i];
        }
        history_count--;
    }
    
    // Ajouter la nouvelle commande
    history[history_count] = malloc(strlen(command) + 1);
    strcpy(history[history_count], command);
    history_count++;
}

// Afficher l'historique
void show_history() {
    printf("\n\033[1;36mHistorique des commandes :\033[0m\n");
    for (int i = 0; i < history_count; i++) {
        printf("  \033[1;33m%3d\033[0m  %s\n", i + 1, history[i]);
    }
    printf("\n");
}

// Sauvegarder l'historique dans un fichier
void save_history_to_file() {
    FILE *file = fopen(HISTORY_FILE, "w");
    if (file == NULL) {
        return;  // Pas de fichier, pas de sauvegarde
    }
    
    for (int i = 0; i < history_count; i++) {
        fprintf(file, "%s\n", history[i]);
    }
    
    fclose(file);
}

// Charger l'historique depuis un fichier
void load_history_from_file() {
    FILE *file = fopen(HISTORY_FILE, "r");
    if (file == NULL) {
        return;  // Pas de fichier, historique vide
    }
    
    char buffer[MAX_COMMAND_LENGTH];
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        // Enlever le saut de ligne
        buffer[strcspn(buffer, "\n")] = 0;
        
        // Ajouter à l'historique
        if (history_count < MAX_HISTORY) {
            history[history_count] = malloc(strlen(buffer) + 1);
            strcpy(history[history_count], buffer);
            history_count++;
        }
    }
    
    fclose(file);
}

// Exécuter une commande de l'historique par numéro
void execute_history_command(int num) {
    if (num < 1 || num > history_count) {
        printf("\033[1;31mErreur : numéro d'historique invalide\033[0m\n");
        return;
    }
    
    char *cmd = history[num - 1];
    printf("Exécution : %s\n", cmd);
    execute_command(cmd);
}

void execute_command(char *command) {
    // Ignorer les commandes vides
    if (strlen(command) == 0) {
        return;
    }
    
    // Ajouter à l'historique (sauf commandes spéciales)
    if (strcmp(command, "history") != 0 && 
        strncmp(command, "!", 1) != 0) {
        add_to_history(command);
    }
    
    // Gestion des commandes internes
    if (strcmp(command, "exit") == 0) {
        save_history_to_file();  // Sauvegarder avant de quitter
        printf("Au revoir ! 👋\n");
        exit(0);
    }
    else if (strcmp(command, "help") == 0) {
        printf("\n\033[1;32mCommandes disponibles :\033[0m\n");
        printf("  \033[1;36mhelp\033[0m     - Afficher cette aide\n");
        printf("  \033[1;36mhistory\033[0m  - Afficher l'historique des commandes\n");
        printf("  \033[1;36m!N\033[0m        - Exécuter la commande N de l'historique\n");
        printf("  \033[1;36m!!\033[0m        - Exécuter la dernière commande\n");
        printf("  \033[1;36mexit\033[0m     - Quitter le shell\n");
        printf("  \033[1;36mclear\033[0m    - Effacer l'écran\n");
        printf("  \033[1;36mdate\033[0m     - Afficher la date et l'heure\n");
        printf("  \033[1;36mpwd\033[0m      - Afficher le répertoire courant\n");
        printf("  \033[1;36mls\033[0m       - Lister les fichiers\n");
        printf("  \033[1;36mvortish\033[0m  - Afficher la bannière\n");
        printf("\n");
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
        // Commande style !5
        int num = atoi(command + 1);
        if (num > 0) {
            execute_history_command(num);
        } else {
            printf("\033[1;31mFormat invalide. Utilisez !N (ex: !5)\033[0m\n");
        }
        return;
    }
    else if (strcmp(command, "clear") == 0) {
        printf("\033[2J\033[1;1H"); // Code ANSI pour effacer l'écran
        return;
    }
    else if (strcmp(command, "vortish") == 0) {
        printf("\n\033[1;35m╔══════════════════════════════════╗\n");
        printf("║        VORTISH SHELL v1.1       ║\n");
        printf("║    (avec historique de commandes)║\n");
        printf("╚══════════════════════════════════╝\033[0m\n\n");
        return;
    }
    
    // Pour les commandes externes, on utilise fork() et execvp()
    pid_t pid = fork();
    
    if (pid == 0) {
        // Processus enfant
        char *args[MAX_ARGS];
        char *token;
        int i = 0;
        char command_copy[MAX_COMMAND_LENGTH];
        
        // Faire une copie car strtok modifie la chaîne
        strcpy(command_copy, command);
        
        // Découper la commande en arguments
        token = strtok(command_copy, " ");
        while (token != NULL && i < MAX_ARGS - 1) {
            args[i] = token;
            token = strtok(NULL, " ");
            i++;
        }
        args[i] = NULL;
        
        // Exécuter la commande
        execvp(args[0], args);
        
        // Si execvp échoue
        printf("\033[1;31mCommande non trouvée : %s\033[0m\n", args[0]);
        exit(1);
    }
    else if (pid > 0) {
        // Processus parent - attendre l'enfant
        wait(NULL);
    }
    else {
        // Erreur de fork
        printf("\033[1;31mErreur lors de l'exécution de la commande\033[0m\n");
    }
}

void run_shell() {
    char command[MAX_COMMAND_LENGTH];
    
    // Charger l'historique depuis le fichier
    load_history_from_file();
    
    while (1) {
        // Afficher le prompt personnalisé
        printf("\033[1;32mvortish@ubuntu\033[0m:\033[1;34m~$\033[0m ");
        
        // Lire la commande
        if (fgets(command, MAX_COMMAND_LENGTH, stdin) == NULL) {
            break;
        }
        
        // Supprimer le saut de ligne
        command[strcspn(command, "\n")] = 0;
        
        // Exécuter la commande
        execute_command(command);
    }
}