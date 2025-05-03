#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <pthread.h>

// Dimensions du labyrinthe et de la fenêtre
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

// Structure d'une cellule du labyrinthe
typedef struct {
    int visite;      // Marqueur pour la résolution
    int estEntree;
    int estSortie;
    int murs[4];     // 0 = passage, 1 = mur
    int sol;         // 1 si la cellule appartient au chemin solution
} Cellule;

// Structure pour passer les paramètres aux threads
typedef struct {
    int x;
    int y;
} ThreadParam;

// Variables globales
Cellule **labyrinthe = NULL;
SDL_Window *fenetre = NULL;
SDL_Renderer *rendu = NULL;

// Mutex pour protéger l'accès aux cellules (visite)
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Prototypes des fonctions classiques
int initialiserSDL();
void initialiserLabyrinthe();
void dessinerLabyrinthe();
void melangerTableau(int *tableau, int taille);
void genererLabyrinthe(int x, int y);
void definirEntreeSortie();
void nettoyer();
void reinitialiserVisiteSolution();

// Prototype de la fonction de résolution avec threads
void *thread_solve(void *arg);

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

    // Affichage initial
    dessinerLabyrinthe();

    // Réinitialisation des marqueurs de visite et de solution
    reinitialiserVisiteSolution();

    // Lancement de la résolution du labyrinthe via threads à partir de l'entrée (0,0)
    pthread_t threadInitial;
    ThreadParam *startParam = malloc(sizeof(ThreadParam));
    startParam->x = 0;
    startParam->y = 0;
    pthread_create(&threadInitial, NULL, thread_solve, startParam);

    void *result;
    pthread_join(threadInitial, &result);

    if ((long)result == 1)
        printf("Solution trouvée par les threads.\n");
    else
        printf("Aucune solution trouvée par les threads.\n");

    // Affichage du labyrinthe avec le chemin solution (en bleu)
    dessinerLabyrinthe();

    // Boucle d'attente jusqu'à fermeture par l'utilisateur
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
    return 0;
}

// ---------------------
// Fonctions d'initialisation SDL
// ---------------------
int initialiserSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL n'a pas pu s'initialiser! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }

    fenetre = SDL_CreateWindow("Labyrinthe - Résolution avec Threads",
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

// ---------------------
// Initialisation du labyrinthe
// ---------------------
void initialiserLabyrinthe() {
    labyrinthe = (Cellule**)malloc(HAUTEUR * sizeof(Cellule*));
    for (int i = 0; i < HAUTEUR; i++) {
        labyrinthe[i] = (Cellule*)malloc(LARGEUR * sizeof(Cellule));
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

// ---------------------
// Fonction de dessin du labyrinthe et du chemin solution
// ---------------------
void dessinerLabyrinthe() {
    SDL_SetRenderDrawColor(rendu, 0, 0, 0, 255);
    SDL_RenderClear(rendu);

    for (int y = 0; y < HAUTEUR; y++) {
        for (int x = 0; x < LARGEUR; x++) {
            int posX = x * TAILLE_CELLULE;
            int posY = y * TAILLE_CELLULE;

            // Affichage du chemin solution en bleu
            if (labyrinthe[y][x].sol) {
                SDL_SetRenderDrawColor(rendu, 0, 0, 255, 255);
                SDL_Rect rectSol = { posX + TAILLE_CELLULE / 4, posY + TAILLE_CELLULE / 4,
                                     TAILLE_CELLULE / 2, TAILLE_CELLULE / 2 };
                SDL_RenderFillRect(rendu, &rectSol);
            }

            // Affichage de l'entrée en vert
            if (labyrinthe[y][x].estEntree) {
                SDL_SetRenderDrawColor(rendu, 0, 255, 0, 255);
                SDL_Rect rectEntree = { posX + 2, posY + 2, TAILLE_CELLULE - 4, TAILLE_CELLULE - 4 };
                SDL_RenderFillRect(rendu, &rectEntree);
            }

            // Affichage de la sortie en rouge
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

// ---------------------
// Algorithme de mélange (Fisher-Yates)
// ---------------------
void melangerTableau(int *tableau, int taille) {
    for (int i = taille - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = tableau[i];
        tableau[i] = tableau[j];
        tableau[j] = temp;
    }
}

// ---------------------
// Génération récursive du labyrinthe
// ---------------------
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
            case OUEST:prochaineX = x - 1; break;
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

// ---------------------
// Définition de l'entrée et de la sortie du labyrinthe
// ---------------------
void definirEntreeSortie() {
    // Définir l'entrée en haut à gauche
    labyrinthe[0][0].estEntree = 1;
    labyrinthe[0][0].murs[NORD] = 0;

    // Définir la sortie au milieu de la dernière colonne
    int sortieY = HAUTEUR / 2;
    labyrinthe[sortieY][LARGEUR - 1].estSortie = 1;
    labyrinthe[sortieY][LARGEUR - 1].murs[EST] = 0;
}

// ---------------------
// Libération des ressources
// ---------------------
void nettoyer() {
    for (int i = 0; i < HAUTEUR; i++) {
        free(labyrinthe[i]);
    }
    free(labyrinthe);

    SDL_DestroyRenderer(rendu);
    SDL_DestroyWindow(fenetre);
    SDL_Quit();
}

// ---------------------
// Réinitialisation des marqueurs de visite et de solution pour la résolution
// ---------------------
void reinitialiserVisiteSolution() {
    for (int i = 0; i < HAUTEUR; i++) {
        for (int j = 0; j < LARGEUR; j++) {
            labyrinthe[i][j].visite = 0;
            labyrinthe[i][j].sol = 0;
        }
    }
}

// ---------------------
// Fonction de résolution du labyrinthe avec threads
// ---------------------
void *thread_solve(void *arg) {
    ThreadParam *param = (ThreadParam*)arg;
    int x = param->x, y = param->y;
    free(param);

    // Vérification des bornes
    if (x < 0 || x >= LARGEUR || y < 0 || y >= HAUTEUR)
        return (void*)0;

    // Protection de l'accès à "visite"
    pthread_mutex_lock(&mutex);
    if (labyrinthe[y][x].visite) {
        pthread_mutex_unlock(&mutex);
        return (void*)0;
    }
    labyrinthe[y][x].visite = 1;
    pthread_mutex_unlock(&mutex);

    // Si nous sommes sur la sortie, marquons la cellule et retournons le succès
    if (labyrinthe[y][x].estSortie) {
        labyrinthe[y][x].sol = 1;
        return (void*)1;
    }

    // Création de threads pour chaque direction accessible (pas de mur)
    pthread_t threads[4];
    int threadCount = 0;

    // Pour chaque direction, on vérifie l'absence de mur et on crée un thread si possible
    if (!labyrinthe[y][x].murs[NORD]) {
        ThreadParam *np = malloc(sizeof(ThreadParam));
        np->x = x;
        np->y = y - 1;
        pthread_create(&threads[threadCount++], NULL, thread_solve, np);
    }
    if (!labyrinthe[y][x].murs[SUD]) {
        ThreadParam *np = malloc(sizeof(ThreadParam));
        np->x = x;
        np->y = y + 1;
        pthread_create(&threads[threadCount++], NULL, thread_solve, np);
    }
    if (!labyrinthe[y][x].murs[EST]) {
        ThreadParam *np = malloc(sizeof(ThreadParam));
        np->x = x + 1;
        np->y = y;
        pthread_create(&threads[threadCount++], NULL, thread_solve, np);
    }
    if (!labyrinthe[y][x].murs[OUEST]) {
        ThreadParam *np = malloc(sizeof(ThreadParam));
        np->x = x - 1;
        np->y = y;
        pthread_create(&threads[threadCount++], NULL, thread_solve, np);
    }

    int success = 0;
    void *retval;
    // Attente de la terminaison des threads fils
    for (int i = 0; i < threadCount; i++) {
        pthread_join(threads[i], &retval);
        if ((long)retval == 1)
            success = 1;
    }

    // Si un des fils a trouvé la solution, marque la cellule actuelle
    if (success) {
        labyrinthe[y][x].sol = 1;
        return (void*)1;
    }

    return (void*)0;
}

