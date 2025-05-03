#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>

/////////////////////////
// Constantes et définitions
/////////////////////////
#define LARGEUR 20
#define HAUTEUR 20
#define TAILLE_CELLULE 25
#define LARGEUR_FENETRE (LARGEUR * TAILLE_CELLULE)
#define HAUTEUR_FENETRE (HAUTEUR * TAILLE_CELLULE)

#define NORD 0
#define SUD 1
#define EST 2
#define OUEST 3

/////////////////////////
// Structure d'une cellule
/////////////////////////
typedef struct {
    int visite;      // Pour le DFS
    int estEntree;
    int estSortie;
    int porte;       // 1 si la cellule est la porte synchronisée, 0 sinon
    int murs[4];     // 1 = mur présent, 0 = passage
    int sol;         // 1 si la cellule fait partie du chemin solution (pour l'affichage)
} Cellule;

/////////////////////////
// Variables globales pour le labyrinthe et SDL
/////////////////////////
Cellule **labyrinthe = NULL;
SDL_Window *fenetre = NULL;
SDL_Renderer *rendu = NULL;

/////////////////////////
// Variables pour enregistrer le chemin solution (pour affichage)
// (facultatif si vous souhaitez aussi tracer la trajectoire, ici on se contente de colorer les cellules)
SDL_Point *solutionPath = NULL;
int solutionPathLength = 0;

/////////////////////////
// Pointeur vers une barrière partagée pour synchroniser la porte
/////////////////////////
pthread_barrier_t *doorBarrier = NULL;

/////////////////////////
// Prototypes des fonctions
/////////////////////////
int initialiserSDL(const char *titre);
void initialiserLabyrinthe();
void dessinerLabyrinthe();
void melangerTableau(int *tableau, int taille);
void genererLabyrinthe(int x, int y);
void definirEntreesSortiesDouble();
void nettoyer();
void reinitialiserVisiteSolution();
int solve_labyrinth_path(int x, int y, int targetX, int targetY);
void dessiner_chemin();  // Optionnel : tracer une ligne reliant les centres du chemin enregistré

/////////////////////////
// Fonctions d'initialisation SDL et du labyrinthe
/////////////////////////
int initialiserSDL(const char *titre) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL n'a pas pu s'initialiser ! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }
    fenetre = SDL_CreateWindow(titre,
                               SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED,
                               LARGEUR_FENETRE,
                               HAUTEUR_FENETRE,
                               SDL_WINDOW_SHOWN);
    if (!fenetre) {
        fprintf(stderr, "La fenêtre n'a pas pu être créée ! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }
    rendu = SDL_CreateRenderer(fenetre, -1, SDL_RENDERER_ACCELERATED);
    if (!rendu) {
        fprintf(stderr, "Le renderer n'a pas pu être créé ! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }
    return 1;
}

void initialiserLabyrinthe() {
    labyrinthe = malloc(HAUTEUR * sizeof(Cellule *));
    for (int i = 0; i < HAUTEUR; i++) {
        labyrinthe[i] = malloc(LARGEUR * sizeof(Cellule));
        for (int j = 0; j < LARGEUR; j++) {
            labyrinthe[i][j].visite = 0;
            labyrinthe[i][j].estEntree = 0;
            labyrinthe[i][j].estSortie = 0;
            labyrinthe[i][j].porte = 0; // par défaut, pas de porte
            labyrinthe[i][j].sol = 0;
            for (int k = 0; k < 4; k++) {
                labyrinthe[i][j].murs[k] = 1;
            }
        }
    }
}

void dessinerLabyrinthe() {
    SDL_SetRenderDrawColor(rendu, 0, 0, 0, 255);
    SDL_RenderClear(rendu);

    for (int y = 0; y < HAUTEUR; y++) {
        for (int x = 0; x < LARGEUR; x++) {
            int posX = x * TAILLE_CELLULE;
            int posY = y * TAILLE_CELLULE;

            // Si la cellule fait partie du chemin solution, coloriez-la en bleu
            if (labyrinthe[y][x].sol) {
                SDL_SetRenderDrawColor(rendu, 0, 0, 255, 255);
                SDL_Rect rectSol = { posX + 2, posY + 2, TAILLE_CELLULE - 4, TAILLE_CELLULE - 4 };
                SDL_RenderFillRect(rendu, &rectSol);
            }
            // Marquer les cellules de porte en magenta pour les repérer
            if (labyrinthe[y][x].porte) {
                SDL_SetRenderDrawColor(rendu, 255, 0, 255, 255);
                SDL_Rect rectPorte = { posX + 4, posY + 4, TAILLE_CELLULE - 8, TAILLE_CELLULE - 8 };
                SDL_RenderFillRect(rendu, &rectPorte);
            }
            // Entrées en vert
            if (labyrinthe[y][x].estEntree) {
                SDL_SetRenderDrawColor(rendu, 0, 255, 0, 255);
                SDL_Rect rectEntree = { posX + 2, posY + 2, TAILLE_CELLULE - 4, TAILLE_CELLULE - 4 };
                SDL_RenderFillRect(rendu, &rectEntree);
            }
            // Sorties en rouge
            if (labyrinthe[y][x].estSortie) {
                SDL_SetRenderDrawColor(rendu, 255, 0, 0, 255);
                SDL_Rect rectSortie = { posX + 2, posY + 2, TAILLE_CELLULE - 4, TAILLE_CELLULE - 4 };
                SDL_RenderFillRect(rendu, &rectSortie);
            }
            // Dessiner les murs en blanc
            SDL_SetRenderDrawColor(rendu, 255, 255, 255, 255);
            if (labyrinthe[y][x].murs[NORD])
                SDL_RenderDrawLine(rendu, posX, posY, posX + TAILLE_CELLULE, posY);
            if (labyrinthe[y][x].murs[SUD])
                SDL_RenderDrawLine(rendu, posX, posY + TAILLE_CELLULE, posX + TAILLE_CELLULE, posY + TAILLE_CELLULE);
            if (labyrinthe[y][x].murs[OUEST])
                SDL_RenderDrawLine(rendu, posX, posY, posX, posY + TAILLE_CELLULE);
            if (labyrinthe[y][x].murs[EST])
                SDL_RenderDrawLine(rendu, posX + TAILLE_CELLULE, posY, posX + TAILLE_CELLULE, posY + TAILLE_CELLULE);
        }
    }
    SDL_RenderPresent(rendu);
}

void melangerTableau(int *tableau, int taille) {
    for (int i = taille - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = tableau[i];
        tableau[i] = tableau[j];
        tableau[j] = temp;
    }
}

void genererLabyrinthe(int x, int y) {
    labyrinthe[y][x].visite = 1;

    int directions[] = { NORD, SUD, EST, OUEST };
    melangerTableau(directions, 4);

    for (int i = 0; i < 4; i++) {
        int prochaineX = x, prochaineY = y;
        switch (directions[i]) {
            case NORD: prochaineY = y - 1; break;
            case SUD:  prochaineY = y + 1; break;
            case EST:  prochaineX = x + 1; break;
            case OUEST: prochaineX = x - 1; break;
        }
        if (prochaineX >= 0 && prochaineX < LARGEUR &&
            prochaineY >= 0 && prochaineY < HAUTEUR &&
            !labyrinthe[prochaineY][prochaineX].visite)
        {
            switch (directions[i]) {
                case NORD:
                    labyrinthe[y][x].murs[NORD] = 0;
                    labyrinthe[prochaineY][prochaineX].murs[SUD] = 0;
                    break;
                case SUD:
                    labyrinthe[y][x].murs[SUD] = 0;
                    labyrinthe[prochaineY][prochaineX].murs[NORD] = 0;
                    break;
                case EST:
                    labyrinthe[y][x].murs[EST] = 0;
                    labyrinthe[prochaineY][prochaineX].murs[OUEST] = 0;
                    break;
                case OUEST:
                    labyrinthe[y][x].murs[OUEST] = 0;
                    labyrinthe[prochaineY][prochaineX].murs[EST] = 0;
                    break;
            }
            genererLabyrinthe(prochaineX, prochaineY);
        }
    }
}

// Pour le labyrinthe double : deux entrées sur le côté gauche et deux sorties sur le côté droit.
// Nous ajoutons ici une porte sur le chemin menant à la sortie.
// Par exemple, nous positionnons la porte à la cellule (HAUTEUR/2, LARGEUR-2).
void definirEntreesSortiesDouble() {
    // Entrée joueur 1 en haut à gauche
    labyrinthe[0][0].estEntree = 1;
    labyrinthe[0][0].murs[OUEST] = 0;
    // Entrée joueur 2 en bas à gauche
    labyrinthe[HAUTEUR - 1][0].estEntree = 1;
    labyrinthe[HAUTEUR - 1][0].murs[OUEST] = 0;

    // Sortie joueur 1 en haut à droite
    labyrinthe[0][LARGEUR - 1].estSortie = 1;
    labyrinthe[0][LARGEUR - 1].murs[EST] = 0;
    // Sortie joueur 2 en bas à droite
    labyrinthe[HAUTEUR - 1][LARGEUR - 1].estSortie = 1;
    labyrinthe[HAUTEUR - 1][LARGEUR - 1].murs[EST] = 0;

    // Ajouter une porte sur le chemin commun menant à la sortie.
    // Par exemple, positionnons la porte à la cellule centrale de la colonne LARGEUR-2.
    int doorY = HAUTEUR / 2;
    int doorX = LARGEUR - 2;
    labyrinthe[doorY][doorX].porte = 1;
    // Pour renforcer l'effet (optionnel), on peut supprimer le mur EST de la porte pour faciliter le passage.
    labyrinthe[doorY][doorX].murs[EST] = 0;
}

void nettoyer() {
    for (int i = 0; i < HAUTEUR; i++) {
        free(labyrinthe[i]);
    }
    free(labyrinthe);
    if (solutionPath)
        free(solutionPath);
    if (doorBarrier)
        munmap(doorBarrier, sizeof(pthread_barrier_t));
    SDL_DestroyRenderer(rendu);
    SDL_DestroyWindow(fenetre);
    SDL_Quit();
}

void reinitialiserVisiteSolution() {
    for (int i = 0; i < HAUTEUR; i++) {
        for (int j = 0; j < LARGEUR; j++) {
            labyrinthe[i][j].visite = 0;
            labyrinthe[i][j].sol = 0;
        }
    }
    solutionPathLength = 0;
    solutionPath = realloc(solutionPath, LARGEUR * HAUTEUR * sizeof(SDL_Point));
}

/////////////////////////
// DFS avec enregistrement du chemin solution
// Si la cellule contient une porte (porte == 1), le joueur attend que l'autre arrive.
int solve_labyrinth_path(int x, int y, int targetX, int targetY) {
    if (x < 0 || x >= LARGEUR || y < 0 || y >= HAUTEUR)
        return 0;
    if (labyrinthe[y][x].visite)
        return 0;

    labyrinthe[y][x].visite = 1;

    // Enregistrer la cellule dans le chemin (optionnel)
    solutionPath[solutionPathLength].x = x;
    solutionPath[solutionPathLength].y = y;
    solutionPathLength++;

    // Si nous avons atteint la sortie, marquer et retourner succès
    if (x == targetX && y == targetY) {
        labyrinthe[y][x].sol = 1;
        return 1;
    }

    // Si la cellule courante est une porte, synchroniser les deux joueurs.
    if (labyrinthe[y][x].porte) {
        // Afficher un message sur la fenêtre (optionnel)
        printf("Processus (PID %d) atteint la porte en (%d, %d)\n", getpid(), x, y);
        // La barrière doit être utilisée : le processus attend que l'autre joueur arrive.
        pthread_barrier_wait(doorBarrier);
        // On peut, par exemple, colorer la porte différemment en la marquant dans le champ sol.
        labyrinthe[y][x].sol = 1;
    }

    int found = 0;
    if (!labyrinthe[y][x].murs[NORD] && solve_labyrinth_path(x, y - 1, targetX, targetY))
        found = 1;
    else if (!labyrinthe[y][x].murs[SUD] && solve_labyrinth_path(x, y + 1, targetX, targetY))
        found = 1;
    else if (!labyrinthe[y][x].murs[EST] && solve_labyrinth_path(x + 1, y, targetX, targetY))
        found = 1;
    else if (!labyrinthe[y][x].murs[OUEST] && solve_labyrinth_path(x - 1, y, targetX, targetY))
        found = 1;

    if (found)
        labyrinthe[y][x].sol = 1;
    else
        solutionPathLength--; // Backtracking
    return found;
}

/////////////////////////
// (Optionnel) Fonction pour dessiner le chemin solution sur l'interface
// Ici, nous tracons une ligne jaune reliant les centres des cellules du chemin enregistré.
void dessiner_chemin() {
    if (solutionPathLength < 2)
        return;
    SDL_Point *points = malloc(solutionPathLength * sizeof(SDL_Point));
    for (int i = 0; i < solutionPathLength; i++) {
        points[i].x = solutionPath[i].x * TAILLE_CELLULE + TAILLE_CELLULE / 2;
        points[i].y = solutionPath[i].y * TAILLE_CELLULE + TAILLE_CELLULE / 2;
    }
    SDL_SetRenderDrawColor(rendu, 255, 255, 0, 255); // Jaune
    SDL_RenderDrawLines(rendu, points, solutionPathLength);
    SDL_RenderPresent(rendu);
    free(points);
}

/////////////////////////
// Programme principal avec synchronisation de la porte
// Deux processus (joueurs) vont résoudre le labyrinthe et, lorsqu'ils atteignent la porte commune, ils se synchroniseront.
/////////////////////////
int main(int argc, char **argv) {
    srand(time(NULL));

    // Initialisation SDL dans le processus parent
    if (!initialiserSDL("Labyrinthe Double - Synchronisation de la Porte")) {
        fprintf(stderr, "Erreur lors de l'initialisation de SDL.\n");
        return EXIT_FAILURE;
    }

    // Initialisation du labyrinthe
    initialiserLabyrinthe();
    genererLabyrinthe(0, 0);
    definirEntreesSortiesDouble();

    // Création d'une barrière partagée pour synchroniser les deux joueurs
    doorBarrier = mmap(NULL, sizeof(pthread_barrier_t), PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (doorBarrier == MAP_FAILED) {
        perror("Erreur mmap pour la barrière");
        exit(EXIT_FAILURE);
    }
    pthread_barrierattr_t barrierAttr;
    pthread_barrierattr_init(&barrierAttr);
    pthread_barrierattr_setpshared(&barrierAttr, PTHREAD_PROCESS_SHARED);
    pthread_barrier_init(doorBarrier, &barrierAttr, 2);
    pthread_barrierattr_destroy(&barrierAttr);

    // Affichage initial du labyrinthe généré
    dessinerLabyrinthe();
    SDL_Delay(2000); // Permet de visualiser le labyrinthe avant résolution

    // Création des deux processus joueurs
    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("Erreur de fork");
        exit(EXIT_FAILURE);
    }
    if (pid1 == 0) {
        // Processus Joueur 1 : entrée en haut à gauche (0,0) -> sortie en haut à droite (0, LARGEUR-1)
        reinitialiserVisiteSolution();
        int res = solve_labyrinth_path(0, 0, 0, LARGEUR - 1);
        if (res)
            printf("Joueur 1 (PID %d) a résolu le labyrinthe !\n", getpid());
        else
            printf("Joueur 1 (PID %d) n'a pas trouvé de solution.\n", getpid());
        dessiner_chemin(); // Optionnel : afficher le chemin sous forme de ligne jaune
        SDL_Delay(5000);
        nettoyer();
        exit(EXIT_SUCCESS);
    }

    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("Erreur de fork");
        exit(EXIT_FAILURE);
    }
    if (pid2 == 0) {
        // Processus Joueur 2 : entrée en bas à gauche (HAUTEUR-1,0) -> sortie en bas à droite (HAUTEUR-1, LARGEUR-1)
        reinitialiserVisiteSolution();
        int res = solve_labyrinth_path(HAUTEUR - 1, 0, HAUTEUR - 1, LARGEUR - 1);
        if (res)
            printf("Joueur 2 (PID %d) a résolu le labyrinthe !\n", getpid());
        else
            printf("Joueur 2 (PID %d) n'a pas trouvé de solution.\n", getpid());
        dessiner_chemin();
        SDL_Delay(5000);
        nettoyer();
        exit(EXIT_SUCCESS);
    }

    // Le processus parent attend la fin des deux enfants
    wait(NULL);
    wait(NULL);

    nettoyer();
    return EXIT_SUCCESS;
}
