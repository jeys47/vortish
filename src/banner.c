#include <stdio.h>
#include "banner.h"

void display_banner() {
    printf("\033[1;36m"); // Couleur cyan
    
    printf("\n WELCOME TO \n");
    printf("\n");
    printf("██╗   ██╗ ██████╗ ██████╗ ████████╗██╗███████╗██╗  ██╗\n");
    printf("██║   ██║██╔═══██╗██╔══██╗╚══██╔══╝██║██╔════╝██║  ██║\n");
    printf("██║   ██║██║   ██║██████╔╝   ██║   ██║███████╗███████║\n");
    printf("╚██╗ ██╔╝██║   ██║██╔══██╗   ██║   ██║╚════██║██╔══██║\n");
    printf(" ╚████╔╝ ╚██████╔╝██║  ██║   ██║   ██║███████║██║  ██║\n");
    printf("  ╚═══╝   ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚═╝╚══════╝╚═╝  ╚═╝\n");
    
    printf("\033[1;33m"); // Couleur jaune
    printf("══════════════════════════════════════════════════════\n");
    printf("  Shell Éducatif - Projet Fun - Version 1.0\n");
    printf("  Architecture : Linux/Unix | C | Status : running...\n");

    printf("  Tapez 'exit' pour quitter, 'help' pour l'aide\n");
    printf("══════════════════════════════════════════════════════\n");
    printf("\033[0m"); // Réinitialiser la couleur
    
    printf("\n");
}