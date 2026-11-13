# Déboguer les animations d'invocation (espers)

Guide pratique pour vérifier et corriger une animation d'esper du moteur. Le
fonctionnement de l'interpréteur est décrit dans
[rendu-et-animations.md](rendu-et-animations.md) ; ce document explique comment
travailler dessus.

## 1. Repères

- Une invocation est l'attaque **54 + n**, où n est l'indice de l'esper
  (Ramuh 0, Ifrit 1, Shiva 2, Siren 3, Terrato 4, Shoat 5, Maduin 6, Bismarck 7,
  Stray 8, Palidor 9, Tritoch 10, Odin 11, Raiden 12, Bahamut 13, Alexandre 14,
  Crusader 15, Ragnarok 16, Kirin 17, Zone Seek 18, Carbuncle 19, Phantom 20,
  Seraph 21, Golem 22, Unicorn 23, Fenrir 24, Lilith 25, Phoenix 26).
  Alexandre est donc l'attaque 68, Odin la 65.
- Le code est dans `engine/src/battle/attack_anim_player.{hpp,cpp}` (interpréteur
  des scripts) et `engine/src/battle/battle_screen.{hpp,cpp}` (composition de
  l'écran). Les données viennent de `engine/src/data/attack_anim.*`.
- Les constantes d'ajustement sont dans `engine/src/constants.hpp`
  (`FF6_JUSTICE_*`, `FF6_BATTLE_VEIL_*`, `FF6_BATTLE_BG1_PIVOT`…).
- Référence : le désassemblage `everything8215/ff6`, commit `813013276c`,
  dossier `src/btlgfx/`. Les fonctions d'initialisation propres à chaque esper
  (`kEsperSheets`, `initType`) sont dans `attack_anim_player.cpp`.

## 2. Construire et lancer

```
cmake --build build/engine -j 8
./build/engine/ff6 output/pack --party terra,edgar,celes,strago --battle 5 --bg 5 \
    --play 68 --capture /tmp/x.bmp --after 140
```

- `--play n` joue l'attaque n ; `--after n` attend n images avant de capturer
  (le script avance d'une image par image du jeu, 60 par seconde).
- `--battle 5 --bg 5` donne le décor de la vidéo de référence (désert).
- `--capture x.bmp` écrit un BMP 24 bits 256×224.
- Pour obtenir une série, boucler sur `--after` (par exemple 0, 10, 20…) dans un
  script, plutôt que d'ouvrir la fenêtre.

## 3. Voir les cadres d'une animation

```
./build/engine/ff6 output/pack --anim 68 --layer 1 --capture /tmp/sheet.bmp
```

`--layer 0` sprites, `--layer 1` bg1, `--layer 2` bg3. La planche montre chaque
cadre numéroté ; elle sert à vérifier les découpages, les palettes et les cadres
qui manquent. Si une couche est vide ou bleue, chercher une palette chargée par
script (`$80/$2A`, `$2B`, `$2C`).

## 4. Commandes ignorées

`AttackAnimPlayer::skipped()` renvoie les paires (commande, nombre) que
l'interpréteur n'a pas su exécuter. Si une animation paraît fausse, regarder
d'abord cette liste : une commande ignorée est la cause la plus fréquente.
La liste générale de ce qui reste est en fin de `rendu-et-animations.md`.

## 5. Comparer avec le jeu

### Avec une vidéo existante

La vidéo d'Alexandre (`alexander.mov`) est décalée d'environ 12 images par
rapport au moteur. Pour extraire l'image équivalente :

```
ffmpeg -ss <t> -i alexander.mov -frames:v 1 \
    -vf "crop=1306:1143:159:78,scale=256:224:flags=neighbor" /tmp/v.png
```

avec `t = 8 + (f + 12 - 30) / 60` pour l'image f du moteur. Vérifier d'abord le
recadrage : le haut du HUD doit tomber sur la ligne 152.

### Avec Ares (v148) : vue d'ensemble

Ares (émulateur Super Famicom, `ares.app` à la racine du dépôt, non suivi par git)
sert à produire une référence pour chaque esper : on charge une savestate prise en
combat, on y équipe l'esper, on déclenche l'invocation, on enregistre la fenêtre en
vidéo, puis on compare image par image avec le moteur. La chaîne est :

```
savestate .bs1  --patch WRAM-->  .bs1 modifiée  --Ares + touches-->  vidéo .mov
      --ffmpeg-->  images 256×224  --comparaison-->  moteur (--play 54+n)
```

Fichiers à la racine, tous ignorés du dépôt (ressources, ne pas commiter) :

| Fichier | Rôle |
|---|---|
| `Final Fantasy 6 [US].smc` | ROM (déjà exclue par `.gitignore`) |
| `Final Fantasy 6 [US].ram` | sauvegarde du jeu (SRAM, 8192 octets) avec tous les espers débloqués |
| `Final Fantasy 6 [US].bs1` | savestate de l'emplacement 1, en combat ; c'est le fichier qu'on patche |
| `Final Fantasy 6 [US].bs1.battle` | copie intacte de la savestate de combat, point de départ de chaque patch |
| `*.bak` | copies de sécurité du `.ram` et d'un ancien `.bs1` |
| `alexander.mov` | enregistrement de référence d'Alexandre |

### Lancer et capturer

Ce qui a été éprouvé :

- Lancement avec une savestate : `ares.app/Contents/MacOS/ares --system
  "Super Famicom" --no-file-prompt --setting Input/Defocus=Play --save-state 1
  "Final Fantasy 6 [US].smc"` (le réglage `Defocus=Play` évite la pause quand la
  fenêtre perd le focus).
- Capture de la fenêtre seule, même derrière d'autres : lister les fenêtres
  (CoreGraphics via `osascript -l JavaScript`) puis `screencapture -x -o -l<id>`.
  Vidéo : `screencapture -v -V <secondes> -l<id> out.mov` (60 images/s). La
  fenêtre de 766×628 points donne 1532×1256 pixels ; la zone de jeu est corrigée
  du format d'image (≈ 1532×1138), à redimensionner en 256×224 avec
  `flags=neighbor`.
- Permissions macOS : Enregistrement d'écran pour la capture, **Accessibilité**
  pour envoyer des touches (`CGEventPost` depuis `osascript`). Sans elle les
  touches sont ignorées sans erreur.
- Touches du clavier d'Ares (pilote Quartz) : flèches pour la croix, A = `S`,
  B = `D`, X = `W`, Y = `E`, Select = `.`, Start = `/`. Une touche tenue moins
  de ~40 ms n'est pas vue ; 50 à 100 ms passent, mais un appui sur A peut
  enchaîner plusieurs étapes du menu.
- Lecture de l'écran sans voir l'image : un petit programme Swift (framework
  Vision, `VNRecognizeTextRequest`) lit les textes du menu ; la détection de
  changement par différence de pixels sur une zone suffit pour suivre le curseur.

### Équiper un esper dans la savestate

Le fichier `.bs1` d'Ares n'est pas compressé. La WRAM du SNES y commence au
décalage `0x228C`. Pour Terra (emplacement 0) :

| Adresse WRAM | Contenu |
|---|---|
| `$161E` | esper équipé (structure du personnage) |
| `$3344` | esper équipé en combat |
| `$3345` | drapeaux de ciblage de l'esper |
| `$208E` à `$2091` | première entrée de la liste de sorts : esper, `$FF`, drapeaux, coût en PM |

Les drapeaux de ciblage de l'attaque 54 + n sont le premier octet de sa fiche, à
`C4/6AC0 + 14 × (54 + n)` (ajouter 512 octets si la ROM a un en-tête). Toujours
garder une copie de la savestate d'origine et repartir de celle-ci pour chaque
essai.

Le patch a été écrit en Python (lecture de la savestate, écriture des octets ci-dessus
à `0x228C + adresse`, réécriture du fichier). Vérifications faites : la structure des
personnages est à `$1600` (0x25 octets par personnage, le nom de Terra y est en
clair), le champ de bits des espers à `$1A69` vaut `FF FF FF 07` (27 espers) et la
savestate de combat a `$11E0 = 0xC5` (groupe), `$ECB8 = 5` (fond).

### Formats de sauvegarde

- **`.ram`** : SRAM de 8192 octets, trois emplacements de 0xA00 octets. Dans un
  emplacement, le champ de bits des espers est à +0x469 et une somme de contrôle de
  16 bits (petit-boutien) à +0x9FE. Les noms des personnages (codage du jeu, 0x25
  octets d'écart) permettent de retrouver les structures.
- **`.bs1`** : savestate d'Ares (signature `BST1`), non compressée, d'environ 274 Ko :
  copie de la SRAM, WRAM (128 Ko, début à 0x228C), VRAM, OAM, CGRAM et mémoire audio.
  Comme VRAM, OAM et CGRAM y sont en clair, on peut en tirer la vérité terrain du PPU
  (c'est l'idée du plan « modèle de PPU », pas encore réalisée).

### Le serveur GDB d'Ares : essayé, inutilisable

Ares embarque un serveur de débogage GDB (port 9123 par défaut, désactivé). Il
aurait permis de lire et d'écrire la mémoire pendant l'exécution. Essai :

```
ares --system "Super Famicom" --no-file-prompt \
     --setting DebugServer/Enabled=true --setting DebugServer/Port=9123 \
     --setting DebugServer/UseIPv4=true "Final Fantasy 6 [US].smc"
```

puis un petit client Python qui ouvre la socket et envoie le paquet `qSupported`.
Résultat : la connexion TCP est acceptée, mais Ares la ferme dès le premier paquet,
sans réponse (cause non vérifiée ; le serveur semble ne pas gérer la Super Famicom
dans cette version). Aucune lecture ni écriture de mémoire n'est donc possible par ce canal ;
la modification passe par le fichier de savestate, hors ligne. Si une version
ultérieure d'Ares corrige cela, ce serait le moyen le plus propre.

### Scripts d'automatisation

Ils étaient dans `/tmp` et ne sont pas dans le dépôt ; voici ce qu'ils faisaient, pour
les refaire :

- **Lanceur** : tue Ares, recopie la savestate patchée, lance Ares avec `open -a`
  (pour qu'il passe au premier plan), attend 8 s, puis exécute une suite de lignes
  `w <s>` (attendre), `k <code> [durée]` (touche), `s <nom>` (capture PNG), `v <nom> <s>`
  (vidéo en arrière-plan), puis tue Ares.
- **Liste des fenêtres** : script JXA (`osascript -l JavaScript`) qui appelle
  `CGWindowListCopyWindowInfo` et affiche numéro, titre et rectangle des fenêtres du
  processus `ares`. Le numéro de la fenêtre titrée « Final Fantasy 6 [US] » est passé à
  `screencapture -l`.
- **Envoi de touche** : script JXA qui crée un événement clavier
  (`CGEventCreateKeyboardEvent`) avec une source `HIDSystemState` et l'envoie par
  `CGEventPost(kCGHIDEventTap, …)`, attend la durée voulue, puis envoie le relâchement.
  Les codes sont ceux de macOS (flèche bas 125, `S` 1, `D` 2, `W` 13, `E` 14).
  Vérifier `AXIsProcessTrusted()` : s'il renvoie faux, Accessibilité n'est pas accordée.
- **OCR** : programme Swift (`VNRecognizeTextRequest`, niveau « précis », sans
  correction de langue) qui affiche le texte reconnu et sa boîte ; sert à savoir quel
  menu est ouvert (Fight / Magic / Morph / Item, nom de l'ennemi visé, PV).
- **Analyse de vidéo** : `ffmpeg -i x.mov -vf fps=20 f_%03d.png`, différence moyenne de
  pixels entre images successives sur la zone du menu pour dater les changements, rendu
  ASCII d'une zone quand on ne peut pas afficher l'image.

### Comment le curseur et les touches se comportent

Par différence de pixels, un appui sur la flèche bas déplace bien le curseur d'une ligne
dans le menu de Terra, donc les touches arrivent. Par contre un appui sur A, avec ou
sans le patch, ouvre une fenêtre d'une ligne (« Mesosaur ») puis passe au personnage
suivant en quelques dixièmes de seconde, sans liste de sorts visible. La cause n'est pas
élucidée (répétition de touche du jeu, sélection automatique de la cible unique, ou
durée de pression mal mesurée). Tant que ce n'est pas résolu, le lancement de l'esper par
les menus n'est pas fiable.

Pistes, par ordre de préférence :

1. Injecter directement l'action dans les files du combat (`$3420` à `$3920`, voir
   `battle_ram.inc` du désassemblage) au lieu de passer par le menu.
2. Comparer plusieurs savestates réelles : en prendre une dans le menu Magic de Terra
   avec l'esper équipé par le jeu, et observer les octets qui diffèrent.
3. Tester d'autres durées de pression (40 à 60 ms) et d'autres touches (B pour annuler).

## 6. Méthode de correction

1. Jouer l'animation, repérer l'image où l'écart apparaît.
2. Lister `skipped()` et la planche des cadres de la couche concernée.
3. Lire le code du jeu qui **consomme** la valeur (les commentaires du
   désassemblage se trompent parfois, voir §7 de `rendu-et-animations.md`).
4. Corriger, puis recapturer à la même image et comparer.
5. Si l'écart est mesuré mais sans cause trouvée dans le code du jeu, le dire dans
   le code et dans la doc, avec une constante nommée `FF6_*`.
6. Lancer les tests :
   `FF6_PACK=$PWD/output/pack ./build/engine/ff6_tests`.
