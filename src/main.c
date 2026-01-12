#include <stdio.h>
#include <stdlib.h>
#include "banner.h"
#include "shell.h"

int main() {
    // Afficher la bannière
    display_banner();
    
    // Lancer le shell
    run_shell();
    
    return 0;
}