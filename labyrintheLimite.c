#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <pthread.h>

//////////////////////
// Définition des constantes
//////////////////////
#define LARGEUR 20
#define HAUTEUR 20
#define TAILLE_CELLULE 25
#define LARGEUR_FENETRE (LARGEUR * TAILLE_CELLULE)
#define HAUTEUR_FENETRE (HAUTEUR * TAILLE_CELLULE)

// Directions
#define NORD 0
#define SUD 1
#define EST 2
#define OUEST 3

// Nombre maximum de threads à utiliser
#define MAX_THREADS 4

//////////////////////
// Structures
//////////////////////
typedef struct {
    int visite;      // Pour la résolution
    int estEntree;
    int estSortie;
    int murs[4];     // 1 = mur présent, 0 = passage
    int sol;         // 1 si la cellule appartient au chemin solution
} Cellule;

typedef struct {
    int x;
    int y;
} ThreadParam;

//////////////////////
// Variables Globales
//////////////////////
Cellule **labyrinthe = NULL;
SDL_Window *fenetre = NULL;
SDL_Renderer *rendu = NULL;

// Mutex pour protéger l'accès aux cellules
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Mutex et compteur pour limiter le nombre de threads simultanés
pthread_mutex_t thread_count_mutex = PTHREAD_MUTEX_INITIALIZER;
int active_thread_count = 0;

//////////////////////
// Prototypes de fonctions
//////////////////////
int initialiserSDL();
void initialiserLabyrinthe();
void dessinerLabyrinthe();
void melangerTableau(int *tableau, int taille);
void genererLabyrinthe(int x, int y);
void definirEntreeSortie();
void nettoyer();
void reinitialiserVisiteSolution();

// Fonctions de résolution
int solve_cell(int x, int y);
void *thread_solve_limited(void *arg);

//////////////////////
// Fonction main
//////////////////////
int main(int argc, char **argv) {
    srand(time(NULL));

    // Initialisation de SDL
    if (!initialiserSDL()) {
        printf("Erreur lors de l'initialisation de SDL.\n");
        return EXIT_FAILURE;
    }

    // Génération du labyrinthe
    initialiserLabyrinthe();
    genererLabyrinthe(0, 0);
    definirEntreeSortie();

    // Affichage initial du labyrinthe généré
    dessinerLabyrinthe();

    // Réinitialisation pour la phase de résolution
    reinitialiserVisiteSolution();

    // Démarrage de la résolution à partir de l'entrée (0,0) via un thread initial
    pthread_t initial_thread;
    ThreadParam *init_param = malloc(sizeof(ThreadParam));
    init_param->x = 0;
    init_param->y = 0;
    pthread_create(&initial_thread, NULL, thread_solve_limited, init_param);

    void *result;
    pthread_join(initial_thread, &result);

    if ((long)result == 1)
        printf("Solution trouvée avec la version limitée par mutex.\n");
    else
        printf("Aucune solution trouvée.\n");

    // Affichage final avec le chemin solution (affiché en bleu)
    dessinerLabyrinthe();

    // Boucle d'attente pour garder la fenêtre ouverte
    SDL_Event e;
    int quit = 0;
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                quit = 1;
        }
    }

    nettoyer();
    pthread_mutex_destroy(&mutex);
    pthread_mutex_destroy(&thread_count_mutex);

    return 0;
}

//////////////////////
// Implémentation des fonctions SDL et de génération
//////////////////////

// Initialisation de SDL : création de la fenêtre et du renderer
int initialiserSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL n'a pas pu s'initialiser! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }

    fenetre = SDL_CreateWindow("Labyrinthe - Résolution avec threads limités (mutex)",
                               SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED,
                               LARGEUR_FENETRE,
                               HAUTEUR_FENETRE,
                               SDL_WINDOW_SHOWN);
    if (fenetre == NULL) {
        printf("La fenêtre n'a pas pu être créée! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }

    rendu = SDL_CreateRenderer(fenetre, -1, SDL_RENDERER_ACCELERATED);
    if (rendu == NULL) {
        printf("Le renderer n'a pas pu être créé! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }
    return 1;
}

// Allocation et initialisation du labyrinthe
void initialiserLabyrinthe() {
    labyrinthe = malloc(HAUTEUR * sizeof(Cellule*));
    for (int i = 0; i < HAUTEUR; i++) {
        labyrinthe[i] = malloc(LARGEUR * sizeof(Cellule));
        for (int j = 0; j < LARGEUR; j++) {
            labyrinthe[i][j].visite = 0;
            labyrinthe[i][j].estEntree = 0;
            labyrinthe[i][j].estSortie = 0;
            labyrinthe[i][j].sol = 0;
            for (int k = 0; k < 4; k++) {
                labyrinthe[i][j].murs[k] = 1;
            }
        }
    }
}

// Dessine le labyrinthe et met en évidence le chemin solution en bleu
void dessinerLabyrinthe() {
    SDL_SetRenderDrawColor(rendu, 0, 0, 0, 255);
    SDL_RenderClear(rendu);

    for (int y = 0; y < HAUTEUR; y++) {
        for (int x = 0; x < LARGEUR; x++) {
            int posX = x * TAILLE_CELLULE;
            int posY = y * TAILLE_CELLULE;

            // Chemin solution en bleu
            if (labyrinthe[y][x].sol) {
                SDL_SetRenderDrawColor(rendu, 0, 0, 255, 255);
                SDL_Rect rectSol = { posX + TAILLE_CELLULE / 4, posY + TAILLE_CELLULE / 4,
                                     TAILLE_CELLULE / 2, TAILLE_CELLULE / 2 };
                SDL_RenderFillRect(rendu, &rectSol);
            }

            // Entrée en vert
            if (labyrinthe[y][x].estEntree) {
                SDL_SetRenderDrawColor(rendu, 0, 255, 0, 255);
                SDL_Rect rectEntree = { posX + 2, posY + 2, TAILLE_CELLULE - 4, TAILLE_CELLULE - 4 };
                SDL_RenderFillRect(rendu, &rectEntree);
            }

            // Sortie en rouge
            if (labyrinthe[y][x].estSortie) {
                SDL_SetRenderDrawColor(rendu, 255, 0, 0, 255);
                SDL_Rect rectSortie = { posX + 2, posY + 2, TAILLE_CELLULE - 4, TAILLE_CELLULE - 4 };
                SDL_RenderFillRect(rendu, &rectSortie);
            }

            // Dessin des murs en blanc
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

// Mélange le tableau d'entiers (algorithme de Fisher-Yates)
void melangerTableau(int *tableau, int taille) {
    for (int i = taille - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = tableau[i];
        tableau[i] = tableau[j];
        tableau[j] = temp;
    }
}

// Génération récursive du labyrinthe par backtracking
void genererLabyrinthe(int x, int y) {
    labyrinthe[y][x].visite = 1;
    int directions[] = { NORD, SUD, EST, OUEST };
    melangerTableau(directions, 4);

    for (int i = 0; i < 4; i++) {
        int prochaineX = x;
        int prochaineY = y;
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

// Définition de l'entrée et de la sortie
void definirEntreeSortie() {
    // Entrée en haut à gauche
    labyrinthe[0][0].estEntree = 1;
    labyrinthe[0][0].murs[NORD] = 0;

    // Sortie : cellule au milieu de la dernière colonne
    int sortieY = HAUTEUR / 2;
    labyrinthe[sortieY][LARGEUR - 1].estSortie = 1;
    labyrinthe[sortieY][LARGEUR - 1].murs[EST] = 0;
}

// Libération des ressources
void nettoyer() {
    for (int i = 0; i < HAUTEUR; i++) {
        free(labyrinthe[i]);
    }
    free(labyrinthe);
    SDL_DestroyRenderer(rendu);
    SDL_DestroyWindow(fenetre);
    SDL_Quit();
}

// Réinitialisation des marqueurs pour la résolution
void reinitialiserVisiteSolution() {
    for (int i = 0; i < HAUTEUR; i++) {
        for (int j = 0; j < LARGEUR; j++) {
            labyrinthe[i][j].visite = 0;
            labyrinthe[i][j].sol = 0;
        }
    }
}

//////////////////////
// Fonctions de résolution avec limitation par mutex
//////////////////////

// Fonction DFS pour résoudre le labyrinthe, capable de créer un thread si la limite n'est pas atteinte.
// Retourne 1 si un chemin vers la sortie est trouvé, 0 sinon.
int solve_cell(int x, int y) {
    // Vérification des bornes
    if (x < 0 || x >= LARGEUR || y < 0 || y >= HAUTEUR)
        return 0;

    // Marquage de la cellule (protégé par mutex)
    pthread_mutex_lock(&mutex);
    if (labyrinthe[y][x].visite) {
        pthread_mutex_unlock(&mutex);
        return 0;
    }
    labyrinthe[y][x].visite = 1;
    pthread_mutex_unlock(&mutex);

    // Si la cellule est la sortie, marquer et retourner succès
    if (labyrinthe[y][x].estSortie) {
        labyrinthe[y][x].sol = 1;
        return 1;
    }

    int directions[4] = { NORD, SUD, EST, OUEST };
    melangerTableau(directions, 4);

    // Pour chaque direction possible
    for (int i = 0; i < 4; i++) {
        int nextX = x, nextY = y;
        switch (directions[i]) {
            case NORD: nextY = y - 1; break;
            case SUD:  nextY = y + 1; break;
            case EST:  nextX = x + 1; break;
            case OUEST: nextX = x - 1; break;
        }

        // Vérifier s'il n'y a pas de mur dans cette direction
        if (!labyrinthe[y][x].murs[directions[i]]) {
            int found = 0;
            int can_spawn = 0;

            // Vérifier (sous protection) si on peut créer un nouveau thread
            pthread_mutex_lock(&thread_count_mutex);
            if (active_thread_count < MAX_THREADS) {
                active_thread_count++;
                can_spawn = 1;
            }
            pthread_mutex_unlock(&thread_count_mutex);

            if (can_spawn) {
                pthread_t t;
                ThreadParam *param = malloc(sizeof(ThreadParam));
                param->x = nextX;
                param->y = nextY;
                pthread_create(&t, NULL, thread_solve_limited, param);
                void *childResult;
                pthread_join(t, &childResult);

                // Après la fin du thread, décrémenter le compteur
                pthread_mutex_lock(&thread_count_mutex);
                active_thread_count--;
                pthread_mutex_unlock(&thread_count_mutex);

                found = ((long)childResult == 1);
            } else {
                // Si la limite est atteinte, appel séquentiel
                found = solve_cell(nextX, nextY);
            }
            if (found) {
                labyrinthe[y][x].sol = 1;
                return 1;
            }
        }
    }
    return 0;
}

// Wrapper utilisé pour pthread_create. Extrait les coordonnées et appelle solve_cell.
void *thread_solve_limited(void *arg) {
    ThreadParam *param = (ThreadParam *)arg;
    int x = param->x, y = param->y;
    free(param);
    int res = solve_cell(x, y);
    return (void *) (long)res;
}
