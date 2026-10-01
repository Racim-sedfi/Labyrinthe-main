# Labyrinthe — Génération et résolution concurrente de labyrinthes en C

Programmes en C qui génèrent un labyrinthe aléatoire de 20 × 20 cases, l'affichent avec
**SDL2** puis le résolvent. Plusieurs variantes explorent la programmation concurrente :
résolution multithread (**POSIX threads**, mutex), limitation du nombre de threads, et
deux « joueurs » sous forme de **processus** qui progressent de façon indépendante ou
synchronisée (barrière partagée).

## Technologies

- C
- SDL2 (affichage graphique)
- POSIX : `pthread` (threads, mutex, barrières), `fork`, mémoire partagée (`mmap`)

## Fonctionnalités principales

| Fichier                         | Description |
|---------------------------------|-------------|
| `labyrinthe.c`                  | Génération par parcours en profondeur avec retour arrière (backtracking), résolution récursive, chemin solution affiché en bleu. |
| `labyrintheIllimite.c`          | Résolution multithread : un nouveau thread est créé pour chaque direction accessible ; accès aux cases protégé par un mutex. |
| `labyrintheLimite.c`            | Même principe avec au plus 4 threads simultanés (compteur protégé par mutex). |
| `labyrintheBonus.c`             | Labyrinthe à deux entrées et deux sorties (génération et affichage). |
| `labyrintheBonusAsynchrone.c`   | Deux processus joueurs (`fork`) résolvent chacun leur trajet, sans synchronisation. |
| `labyrintheBonusSynchrone.c`    | Deux processus joueurs qui doivent se synchroniser sur une « porte » grâce à une barrière `pthread` partagée entre processus. |

## Compilation et exécution

Prérequis (Linux) : `gcc` et la bibliothèque SDL2 (paquet `libsdl2-dev` sous Debian/Ubuntu).

```bash
# Version de base
gcc -o labyrinthe labyrinthe.c $(pkg-config --cflags --libs sdl2)
./labyrinthe

# Variantes multithread / multiprocessus (même commande, avec -pthread)
gcc -o labyrintheIllimite labyrintheIllimite.c $(pkg-config --cflags --libs sdl2) -pthread
./labyrintheIllimite
```

Chaque fichier est un programme autonome : remplacer le nom du fichier pour compiler une autre variante.
Selon la variante, la fenêtre se ferme en cliquant sur la croix ou après l’affichage du résultat.

## Auteur

Racim Sedfi
