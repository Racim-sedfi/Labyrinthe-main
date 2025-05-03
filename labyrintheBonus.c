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
    int visite;
    int estEntree;
    int estSortie;
    int murs[4];  // murs[0] : NORD, murs[1] : SUD, murs[2] : EST, murs[3] : OUEST
} Cellule;

// Variables globales pour le labyrinthe et SDL
Cellule **labyrinthe = NULL;
SDL_Window *fenetre = NULL;
SDL_Renderer *rendu = NULL;

// Prototypes des fonctions
int initialiserSDL();
void initialiserLabyrinthe();
void dessinerLabyrinthe();
void melangerTableau(int *tableau, int taille);
void genererLabyrinthe(int x, int y);
void definirEntreesSortiesDouble();
void nettoyer();

int main(int argc, char **argv) {
    srand(time(NULL));

    // Initialisation de SDL
    if (!initialiserSDL()) {
        printf("Erreur lors de l'initialisation de SDL.\n");
        return EXIT_FAILURE;
    }

    // Allocation et initialisation du labyrinthe
    initialiserLabyrinthe();

    // Génération du labyrinthe par backtracking (par exemple en partant de (0,0))
    genererLabyrinthe(0, 0);

    // Définition des deux entrées et deux sorties dans la grille
    definirEntreesSortiesDouble();

    // Affichage du labyrinthe généré
    dessinerLabyrinthe();

    // Boucle événementielle pour garder la fenêtre ouverte
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

// ---------------------
// Initialisation de SDL : création de la fenêtre et du renderer
// ---------------------
int initialiserSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL n'a pas pu s'initialiser ! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }

    fenetre = SDL_CreateWindow("Labyrinthe Double - 2 Entrées et 2 Sorties",
                               SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED,
                               LARGEUR_FENETRE,
                               HAUTEUR_FENETRE,
                               SDL_WINDOW_SHOWN);
    if (fenetre == NULL) {
        printf("La fenêtre n'a pas pu être créée ! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }

    rendu = SDL_CreateRenderer(fenetre, -1, SDL_RENDERER_ACCELERATED);
    if (rendu == NULL) {
        printf("Le renderer n'a pas pu être créé ! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }
    return 1;
}

// ---------------------
// Allocation et initialisation du labyrinthe
// ---------------------
void initialiserLabyrinthe() {
    labyrinthe = malloc(HAUTEUR * sizeof(Cellule *));
    for (int i = 0; i < HAUTEUR; i++) {
        labyrinthe[i] = malloc(LARGEUR * sizeof(Cellule));
        for (int j = 0; j < LARGEUR; j++) {
            labyrinthe[i][j].visite = 0;
            labyrinthe[i][j].estEntree = 0;
            labyrinthe[i][j].estSortie = 0;
            // Initialisation de tous les murs comme "présents"
            for (int k = 0; k < 4; k++) {
                labyrinthe[i][j].murs[k] = 1;
            }
        }
    }
}

// ---------------------
// Dessin du labyrinthe via SDL
// ---------------------
void dessinerLabyrinthe() {
    SDL_SetRenderDrawColor(rendu, 0, 0, 0, 255);
    SDL_RenderClear(rendu);

    for (int y = 0; y < HAUTEUR; y++) {
        for (int x = 0; x < LARGEUR; x++) {
            int posX = x * TAILLE_CELLULE;
            int posY = y * TAILLE_CELLULE;

            // Coloration des entrées en vert
            if (labyrinthe[y][x].estEntree) {
                SDL_SetRenderDrawColor(rendu, 0, 255, 0, 255);
                SDL_Rect rectEntree = { posX + 2, posY + 2, TAILLE_CELLULE - 4, TAILLE_CELLULE - 4 };
                SDL_RenderFillRect(rendu, &rectEntree);
            }
            // Coloration des sorties en rouge
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
// Fonction de mélange d'un tableau (algorithme de Fisher-Yates)
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
// Génération récursive du labyrinthe avec l'algorithme de backtracking
// ---------------------
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

        if (prochaineX >= 0 && prochaineX < LARGEUR &&
            prochaineY >= 0 && prochaineY < HAUTEUR &&
            !labyrinthe[prochaineY][prochaineX].visite) {

            // Suppression du mur entre la cellule actuelle et la cellule voisine
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
// Nouveau générateur pour labyrinthe double : définition de 2 entrées et 2 sorties
// ---------------------
void definirEntreesSortiesDouble() {
    // Première entrée : cellule en haut à gauche, ouverture sur le côté OUEST
    labyrinthe[0][0].estEntree = 1;
    labyrinthe[0][0].murs[OUEST] = 0;

    // Deuxième entrée : cellule en bas à gauche, ouverture sur le côté OUEST
    labyrinthe[HAUTEUR - 1][0].estEntree = 1;
    labyrinthe[HAUTEUR - 1][0].murs[OUEST] = 0;

    // Première sortie : cellule en haut à droite, ouverture sur le côté EST
    labyrinthe[0][LARGEUR - 1].estSortie = 1;
    labyrinthe[0][LARGEUR - 1].murs[EST] = 0;

    // Deuxième sortie : cellule en bas à droite, ouverture sur le côté EST
    labyrinthe[HAUTEUR - 1][LARGEUR - 1].estSortie = 1;
    labyrinthe[HAUTEUR - 1][LARGEUR - 1].murs[EST] = 0;
}

// ---------------------
// Libération des ressources et fermeture propre de SDL
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
