#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>

// Définition des dimensions du labyrinthe et de la fenêtre
#define LARGEUR 20
#define HAUTEUR 20
#define TAILLE_CELLULE 25
#define LARGEUR_FENETRE (LARGEUR * TAILLE_CELLULE)
#define HAUTEUR_FENETRE (HAUTEUR * TAILLE_CELLULE)

// Définition des directions
#define NORD 0
#define SUD 1
#define EST 2
#define OUEST 3

// Structure d'une cellule du labyrinthe
typedef struct {
    int visite;      // Sert pour la génération puis pour la résolution (sera réinitialisé avant le DFS de résolution)
    int estEntree;
    int estSortie;
    int murs[4];     // murs[0] : NORD, murs[1] : SUD, murs[2] : EST, murs[3] : OUEST (1 = mur présent, 0 = passage)
    int sol;         // 1 si la cellule fait partie du chemin solution, 0 sinon
} Cellule;

// Variables globales
Cellule **labyrinthe = NULL;
SDL_Window *fenetre = NULL;
SDL_Renderer *rendu = NULL;

// Prototypes de fonctions
int initialiserSDL();
void initialiserLabyrinthe();
void dessinerLabyrinthe();
void melangerTableau(int *tableau, int taille);
void genererLabyrinthe(int x, int y);
void definirEntreeSortie();
void nettoyer();
void reinitialiserVisiteSolution();
int resoudreLabyrinthe(int x, int y);

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

    // Réinitialisation des marqueurs de visite et de solution pour la phase de résolution
    reinitialiserVisiteSolution();

    // Résolution du labyrinthe à partir de l'entrée (0, 0)
    if (resoudreLabyrinthe(0, 0))
        printf("Solution trouvée.\n");
    else
        printf("Aucune solution trouvée.\n");

    // Redessin du labyrinthe avec le chemin solution (affiché en bleu)
    dessinerLabyrinthe();

    // Boucle d'attente jusqu'à la fermeture de la fenêtre par l'utilisateur
    SDL_Event e;
    int quit = 0;
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                quit = 1;
        }
    }

    nettoyer();
    return 0;
}

// Initialisation de SDL, création de la fenêtre et du renderer
int initialiserSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL n'a pas pu s'initialiser! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }

    fenetre = SDL_CreateWindow("Résolution de Labyrinthe", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               LARGEUR_FENETRE, HAUTEUR_FENETRE, SDL_WINDOW_SHOWN);
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

// Fonction de dessin du labyrinthe et du chemin solution
void dessinerLabyrinthe() {
    SDL_SetRenderDrawColor(rendu, 0, 0, 0, 255);
    SDL_RenderClear(rendu);

    for (int y = 0; y < HAUTEUR; y++) {
        for (int x = 0; x < LARGEUR; x++) {
            int posX = x * TAILLE_CELLULE;
            int posY = y * TAILLE_CELLULE;

            // Affichage du chemin solution (en bleu) si la cellule fait partie de la solution
            if (labyrinthe[y][x].sol) {
                SDL_SetRenderDrawColor(rendu, 0, 0, 255, 255);
                SDL_Rect rectSol = { posX + TAILLE_CELLULE / 4, posY + TAILLE_CELLULE / 4,
                                     TAILLE_CELLULE / 2, TAILLE_CELLULE / 2 };
                SDL_RenderFillRect(rendu, &rectSol);
            }

            // Affichage de l'entrée (vert)
            if (labyrinthe[y][x].estEntree) {
                SDL_SetRenderDrawColor(rendu, 0, 255, 0, 255);
                SDL_Rect rectEntree = { posX + 2, posY + 2, TAILLE_CELLULE - 4, TAILLE_CELLULE - 4 };
                SDL_RenderFillRect(rendu, &rectEntree);
            }

            // Affichage de la sortie (rouge)
            if (labyrinthe[y][x].estSortie) {
                SDL_SetRenderDrawColor(rendu, 255, 0, 0, 255);
                SDL_Rect rectSortie = { posX + 2, posY + 2, TAILLE_CELLULE - 4, TAILLE_CELLULE - 4 };
                SDL_RenderFillRect(rendu, &rectSortie);
            }

            // Dessin des murs (en blanc)
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

// Fonction de mélange d'un tableau (algorithme de Fisher-Yates)
void melangerTableau(int *tableau, int taille) {
    for (int i = taille - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = tableau[i];
        tableau[i] = tableau[j];
        tableau[j] = temp;
    }
}

// Génération récursive du labyrinthe à partir de la cellule (x, y)
void genererLabyrinthe(int x, int y) {
    labyrinthe[y][x].visite = 1;

    int directions[] = { NORD, SUD, EST, OUEST };
    melangerTableau(directions, 4);

    for (int i = 0; i < 4; i++) {
        int prochaineX = x;
        int prochaineY = y;

        switch (directions[i]) {
            case NORD:
                prochaineY = y - 1;
                break;
            case SUD:
                prochaineY = y + 1;
                break;
            case EST:
                prochaineX = x + 1;
                break;
            case OUEST:
                prochaineX = x - 1;
                break;
        }

        if (prochaineX >= 0 && prochaineX < LARGEUR && prochaineY >= 0 && prochaineY < HAUTEUR &&
            !labyrinthe[prochaineY][prochaineX].visite)
        {
            // Suppression des murs entre la cellule courante et la suivante
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

// Définition de l'entrée (cellule (0, 0)) et de la sortie (cellule au milieu de la dernière colonne)
void definirEntreeSortie() {
    labyrinthe[0][0].estEntree = 1;
    labyrinthe[0][0].murs[NORD] = 0;

    int sortieY = HAUTEUR / 2;
    labyrinthe[sortieY][LARGEUR - 1].estSortie = 1;
    labyrinthe[sortieY][LARGEUR - 1].murs[EST] = 0;
}

// Libération des ressources allouées et fermeture propre de SDL
void nettoyer() {
    for (int i = 0; i < HAUTEUR; i++) {
        free(labyrinthe[i]);
    }
    free(labyrinthe);

    SDL_DestroyRenderer(rendu);
    SDL_DestroyWindow(fenetre);
    SDL_Quit();
}

// Réinitialisation des marqueurs de visite et de solution avant la résolution
void reinitialiserVisiteSolution() {
    for (int i = 0; i < HAUTEUR; i++) {
        for (int j = 0; j < LARGEUR; j++) {
            labyrinthe[i][j].visite = 0;
            labyrinthe[i][j].sol = 0;
        }
    }
}

// Fonction récursive qui résout le labyrinthe par backtracking
// Retourne 1 si un chemin vers la sortie est trouvé, 0 sinon.
int resoudreLabyrinthe(int x, int y) {
    // Vérification des bornes
    if (x < 0 || x >= LARGEUR || y < 0 || y >= HAUTEUR)
        return 0;

    // Si la cellule a déjà été visitée dans le cadre de la résolution, on arrête
    if (labyrinthe[y][x].visite)
        return 0;

    labyrinthe[y][x].visite = 1;

    // Condition d'arrêt : si la cellule actuelle est la sortie, on marque le chemin et on retourne 1.
    if (labyrinthe[y][x].estSortie) {
        labyrinthe[y][x].sol = 1;
        return 1;
    }

    // Exploration des cellules voisines accessibles (en l'absence de mur)

    // Vers le Nord
    if (!labyrinthe[y][x].murs[NORD]) {
        if (resoudreLabyrinthe(x, y - 1)) {
            labyrinthe[y][x].sol = 1;
            return 1;
        }
    }

    // Vers le Sud
    if (!labyrinthe[y][x].murs[SUD]) {
        if (resoudreLabyrinthe(x, y + 1)) {
            labyrinthe[y][x].sol = 1;
            return 1;
        }
    }

    // Vers l'Est
    if (!labyrinthe[y][x].murs[EST]) {
        if (resoudreLabyrinthe(x + 1, y)) {
            labyrinthe[y][x].sol = 1;
            return 1;
        }
    }

    // Vers l'Ouest
    if (!labyrinthe[y][x].murs[OUEST]) {
        if (resoudreLabyrinthe(x - 1, y)) {
            labyrinthe[y][x].sol = 1;
            return 1;
        }
    }

    return 0;
}
