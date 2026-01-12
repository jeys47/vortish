#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "shell.h"

#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64

void execute_command(char *command) {
    // Ignorer les commandes vides
    if (strlen(command) == 0) {
        return;
    }
    
    // Gestion des commandes internes
    if (strcmp(command, "exit") == 0) {
        printf("Au revoir !\n");
        exit(0);
    }
    else if (strcmp(command, "help") == 0) {
        printf("\n\033[1;32mCommandes disponibles :\033[0m\n");
        printf("  \033[1;36mhelp\033[0m     - Afficher cette aide\n");
        printf("  \033[1;36mexit\033[0m     - Quitter le shell\n");
        printf("  \033[1;36mclear\033[0m    - Effacer l'écran\n");
        printf("  \033[1;36mdate\033[0m     - Afficher la date et l'heure\n");
        printf("  \033[1;36mpwd\033[0m      - Afficher le répertoire courant\n");
        printf("  \033[1;36mls\033[0m       - Lister les fichiers\n");
        printf("  \033[1;36mvortish\033[0m  - Afficher la bannière\n");
        printf("\n");
        return;
    }
    else if (strcmp(command, "clear") == 0) {
        printf("\033[2J\033[1;1H"); // Code ANSI pour effacer l'écran
        return;
    }
    else if (strcmp(command, "vortish") == 0) {
        // On pourrait inclure banner.h ici, mais pour simplifier :
        printf("\n\033[1;35m╔══════════════════════════════════╗\n");
        printf("║        VORTISH SHELL v1.0        ║\n");
        printf("║         Shell Éducatif           ║\n");
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
        
        // Découper la commande en arguments
        token = strtok(command, " ");
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