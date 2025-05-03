#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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
    int murs[4];     // 1 = mur présent, 0 = passage
    int sol;         // (pour affichage, on colore la cellule solution en bleu)
} Cellule;

/////////////////////////
// Variables globales pour le labyrinthe et SDL
/////////////////////////
Cellule **labyrinthe = NULL;
SDL_Window *fenetre = NULL;
SDL_Renderer *rendu = NULL;

/////////////////////////
// Variables pour enregistrer le chemin solution (ordre des cellules)
// Le tableau solutionPath contiendra jusqu'à LARGEUR*HAUTEUR points.
/////////////////////////
SDL_Point *solutionPath = NULL;
int solutionPathLength = 0;

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

// DFS modifié pour enregistrer le chemin solution
int solve_labyrinth_path(int x, int y, int targetX, int targetY);

// Fonction qui dessine le chemin solution sur l'interface
void dessiner_chemin();

/////////////////////////
// Fonctions d'initialisation SDL et génération du labyrinthe
/////////////////////////
int initialiserSDL(const char *titre) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL n'a pas pu s'initialiser ! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }
    fenetre = SDL_CreateWindow(titre,
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

void initialiserLabyrinthe() {
    labyrinthe = malloc(HAUTEUR * sizeof(Cellule *));
    for (int i = 0; i < HAUTEUR; i++) {
        labyrinthe[i] = malloc(LARGEUR * sizeof(Cellule));
        for (int j = 0; j < LARGEUR; j++) {
            labyrinthe[i][j].visite = 0;
            labyrinthe[i][j].estEntree = 0;
            labyrinthe[i][j].estSortie = 0;
            labyrinthe[i][j].sol = 0;
            for (int k = 0; k < 4; k++) {
                labyrinthe[i][j].murs[k] = 1; // Tous les murs sont présents initialement
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

            // Affichage du chemin solution en bleu : chaque cellule marquée sera colorée
            if (labyrinthe[y][x].sol) {
                SDL_SetRenderDrawColor(rendu, 0, 0, 255, 255);
                SDL_Rect rectSol = { posX + 2, posY + 2, TAILLE_CELLULE - 4, TAILLE_CELLULE - 4 };
                SDL_RenderFillRect(rendu, &rectSol);
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

// Pour le labyrinthe double : 2 entrées sur le côté gauche, 2 sorties sur le côté droit
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
}

void nettoyer() {
    for (int i = 0; i < HAUTEUR; i++) {
        free(labyrinthe[i]);
    }
    free(labyrinthe);
    if (solutionPath)
        free(solutionPath);
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
    // Réinitialiser le tableau du chemin solution
    solutionPathLength = 0;
    // Allouer ou réallouer le tableau si nécessaire (taille maximale possible)
    solutionPath = realloc(solutionPath, LARGEUR * HAUTEUR * sizeof(SDL_Point));
}

/////////////////////////
// DFS avec enregistrement du chemin solution
/////////////////////////
int solve_labyrinth_path(int x, int y, int targetX, int targetY) {
    // Vérification des bornes
    if (x < 0 || x >= LARGEUR || y < 0 || y >= HAUTEUR)
        return 0;
    if (labyrinthe[y][x].visite)
        return 0;

    labyrinthe[y][x].visite = 1;
    // Enregistrer la cellule actuelle dans le chemin
    solutionPath[solutionPathLength].x = x;
    solutionPath[solutionPathLength].y = y;
    solutionPathLength++;

    // Vérifier si la sortie est atteinte
    if (x == targetX && y == targetY) {
        labyrinthe[y][x].sol = 1;
        return 1;
    }

    int found = 0;
    // Essayer chacune des quatre directions si le passage est ouvert
    if (!labyrinthe[y][x].murs[NORD] && solve_labyrinth_path(x, y - 1, targetX, targetY))
        found = 1;
    else if (!labyrinthe[y][x].murs[SUD] && solve_labyrinth_path(x, y + 1, targetX, targetY))
        found = 1;
    else if (!labyrinthe[y][x].murs[EST] && solve_labyrinth_path(x + 1, y, targetX, targetY))
        found = 1;
    else if (!labyrinthe[y][x].murs[OUEST] && solve_labyrinth_path(x - 1, y, targetX, targetY))
        found = 1;

    if (found) {
        labyrinthe[y][x].sol = 1;
        return 1;
    }

    // Backtracking : retirer la cellule du chemin
    solutionPathLength--;
    return 0;
}

/////////////////////////
// Fonction pour dessiner graphiquement le chemin solution sur l'interface
// On trace une ligne jaune reliant les centres des cellules enregistrées dans solutionPath
/////////////////////////
void dessiner_chemin() {
    if (solutionPathLength < 2)
        return;
    // Créer un tableau de SDL_Point pour les positions à dessiner
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
// Programme principal avec deux processus (deux joueurs)
// Chaque joueur résout le labyrinthe sur sa copie et le résultat (chemin) est affiché par SDL.
/////////////////////////
int main(int argc, char **argv) {
    srand(time(NULL));

    // Initialisation SDL dans le processus parent
    if (!initialiserSDL("Labyrinthe Double - Deux Joueurs (Sans synchronisation)")) {
        fprintf(stderr, "Erreur lors de l'initialisation de SDL.\n");
        return EXIT_FAILURE;
    }

    // Génération du labyrinthe double
    initialiserLabyrinthe();
    genererLabyrinthe(0, 0);
    definirEntreesSortiesDouble();

    // Affichage initial du labyrinthe généré
    dessinerLabyrinthe();
    SDL_Delay(2000); // Pause de 2 secondes

    // Création des deux processus joueurs
    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("Erreur de fork");
        exit(EXIT_FAILURE);
    }
    if (pid1 == 0) {
        // Processus Joueur 1 : entrée (0,0) -> sortie (0, LARGEUR-1)
        reinitialiserVisiteSolution();
        if (solve_labyrinth_path(0, 0, 0, LARGEUR - 1))
            printf("Joueur 1 a résolu le labyrinthe !\n");
        else
            printf("Joueur 1 n'a pas trouvé de solution.\n");
        // Affichage graphique du chemin solution
        dessiner_chemin();
        SDL_Delay(5000); // Affiche pendant 5 secondes
        nettoyer();
        exit(EXIT_SUCCESS);
    }

    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("Erreur de fork");
        exit(EXIT_FAILURE);
    }
    if (pid2 == 0) {
        // Processus Joueur 2 : entrée (HAUTEUR-1,0) -> sortie (HAUTEUR-1, LARGEUR-1)
        reinitialiserVisiteSolution();
        if (solve_labyrinth_path(HAUTEUR - 1, 0, HAUTEUR - 1, LARGEUR - 1))
            printf("Joueur 2 a résolu le labyrinthe !\n");
        else
            printf("Joueur 2 n'a pas trouvé de solution.\n");
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
