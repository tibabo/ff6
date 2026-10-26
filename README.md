# FF6 : premiers outils pour un port natif

Ce projet contient des outils de validation et d'extraction, ainsi qu'un premier
prototype de moteur natif en C++ (voir "Moteur natif"). Ce n'est pas un
emulateur : aucune instruction 65816 n'est executee. Le jeu complet n'est pas
encore porte.

## Prerequis

- Node.js 24 ou plus recent, disponible sur Windows et macOS.
- Une ROM personnelle non modifiee de Final Fantasy III US revision 1.1
  (Final Fantasy VI), avec ou sans l'en-tete de copie de 512 octets.

Aucun paquet npm, SDK SNES ou telechargement n'est necessaire pour ces outils.
Ils utilisent uniquement la bibliotheque standard de Node.js. SDL2 et un
compilateur C++ ne sont pas necessaires a ce premier jalon.

Profil accepte, apres exclusion de l'en-tete de copie :

| Champ | Valeur |
| --- | --- |
| Taille | 3 145 728 octets |
| CRC32 | `C0FA0464` |
| SHA-256 | `10eccc5d2fab81346dd759f6be478dcb682eef981e8d3d662da176e1f9a996bc` |
| Checksum SNES | `8A60`, avec miroir du dernier Mio |

Une autre revision, une traduction ou une ROM modifiee est refusee. Le SHA-256,
et pas seulement le titre ou le checksum SNES, autorise l'extraction.

## Utilisation

Depuis ce dossier, les memes commandes fonctionnent dans zsh et PowerShell :

```sh
node ff6.mjs inspect --rom "Final Fantasy 6 [US].smc"
node ff6.mjs extract --rom "Final Fantasy 6 [US].smc" --out output/ff3-us-1.1
node ff6.mjs verify --rom "Final Fantasy 6 [US].smc" --out output/ff3-us-1.1
node ff6.mjs check-text --file output/ff3-us-1.1/text/dialogues.json
```

Adapter le nom de la ROM si necessaire. Les chemins contenant des espaces
doivent rester entre guillemets. Les commandes retournent un code de sortie
non nul en cas d'erreur et n'ecrivent jamais dans la ROM source.

`extract` exige un dossier de sortie inexistant, meme si un dossier existant
est vide. Il ne possede pas d'option d'ecrasement. Pour conserver une extraction
de reference et travailler sur une autre, effectuer une seconde extraction
vers un autre dossier sous `output/`.

Les fichiers sont prepares dans un dossier temporaire voisin ; le manifeste
est publie en dernier. Une erreur d'ecriture normale nettoie la nouvelle sortie.
Apres une interruption brutale, un dossier sans manifeste peut etre incomplet :
utiliser une nouvelle destination plutot que reprendre dessus.

## Contenu extrait

L'extraction produit 101 fichiers :

- `manifest.json` : version du format, empreinte de la ROM, adresses sans
  en-tete, dimensions et empreintes des ressources.
- `text/dialogues.json` : les 3 084 entrees de dialogues de terrain, y compris
  les entrees partagees. Ce nombre ne represente pas 3 084 textes uniques.
- `graphics/portraits/` : 19 portraits PNG RGBA de 40 x 40 pixels.
- `graphics/sprite-tiles/` : 14 atlas PNG RGBA de 128 x 96 pixels, contenant
  chacun 181 tuiles de 8 x 8 pixels. Les cases restantes sont transparentes.
- `originals/` : les 33 ensembles de tuiles SNES 4bpp et leurs 33 palettes
  BGR555, conserves sans modification pour inspection et comparaison.

Les portraits sont reassembles selon leur disposition 5 x 5 propre au jeu,
qui differe de l'ordre de stockage. Le manifeste indique
`layout: "ff6-portrait-5x5"` et `sourceTileOrder` : pour chaque case du PNG,
l'indice de la tuile dans le fichier 4bpp original. Les originaux restent inchanges.
Les anciennes extractions aux portraits desordonnes doivent etre regenerees
dans un nouveau dossier, par exemple :

```sh
node ff6.mjs extract --rom "Final Fantasy 6 [US].smc" --out output/ff3-us-1.1-portraits-fixed
```

Les atlas sont des tuiles dans l'ordre de stockage, PAS des planches d'animations
assemblees. Les palettes des personnages sont des palettes de reference ;
le jeu peut les remplacer pendant certains evenements. L'indice de couleur
zero est transparent dans les PNG.

`verify` regenere les ressources en memoire depuis la ROM et compare chaque
fichier attendu, manifeste compris. Modifier un fichier ET son empreinte dans
le manifeste ne contourne donc pas la verification. Les fichiers supplementaires
ne sont pas verifies. Les liens symboliques dans les chemins verifies sont refuses.

## Modifier les dialogues

Dans `text/dialogues.json`, conserver les identifiants et les champs `source`.
Modifier les chaines de caracteres dans `entries[id].tokens`. Exemple invente :

```json
[
  "Bonjour, ",
  { "type": "name", "characterId": 0 },
  " !",
  { "type": "control", "code": 1, "name": "newline", "args": [] },
  "Un texte libre en UTF-8."
]
```

Les chaines peuvent contenir des accents et ne sont pas limitees par la taille
des anciennes banques de texte. Les noms de personnages, icones et commandes
sont des objets distincts. Par exemple, une pause a parametre est representee
par `{"type":"control","code":17,"name":"wait","args":[30]}` ;
la valeur conserve son unite d'origine, elle n'est pas exprimee en millisecondes.

`check-text` valide la structure, les identifiants et les parametres des
commandes. Il ne garantit ni la mise en page ni la logique des dialogues modifies.
`verify` echoue volontairement sur une extraction modifiee : cette commande
sert a controler une reference intacte, pas a approuver un mod.

L'encodage source utilise une table DTE de 128 paires extraite de la ROM.
Les octets source restent disponibles dans `source.encodedHex`. Plusieurs
entrees peuvent viser le meme texte ou en partager la fin : `overlapsNext`
signale les textes depassant le prochain pointeur distinct. L'entree 2125
partage notamment sa fin avec l'entree 2126 ; ce cas est teste.

Aucune reinsertion dans la ROM, compilation de mod ou execution des textes
modifies n'est encore implementee. Les PNG peuvent etre ouverts dans un editeur
d'images, mais le chargement de remplacements HD appartient au futur moteur.

## Moteur natif

Le moteur lit un pack de donnees genere depuis la ROM et joue les cartes du jeu :
decor complet (bg1, bg2, bg3 avec vapeur et brouillard, parallaxe, tuiles et
graphismes animes), PNJ avec leurs collisions, dialogues et choix, declencheurs
d'evenements, portes et sorties entre cartes. Au lancement, la carte par
defaut est l'auberge de Narshe (carte 28), mais `--map n` ouvre n'importe
quelle carte de terrain.

Prerequis supplementaires : CMake 3.20+, un compilateur C++17 et SDL2.

- macOS : `brew install cmake sdl2`.
- Windows : Visual Studio 2022 et SDL2 via vcpkg (`vcpkg install sdl2`), puis
  ajouter `-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake`
  a la configuration CMake.

```sh
node ff6.mjs build-pack --rom "Final Fantasy 6 [US].smc" --out output/pack
cmake -S engine -B build/engine -DCMAKE_BUILD_TYPE=Release
cmake --build build/engine --config Release
FF6_PACK="$PWD/output/pack" ctest --test-dir build/engine -C Release
./build/engine/ff6 output/pack [--map 20] [--party terra,locke] [--music 22] [--load ff6-save.json] [--save ff6-save.json]
```

Sous Windows, l'executable est `build\engine\Release\ff6.exe`, et `SDL2.dll`
doit se trouver a cote de lui (vcpkg l'y copie normalement).

`build-pack` ecrit `common.ff6data` (police, fenetre, sprites, dialogues, script
d'evenements), `audio.ff6data` (echantillons BRR, enveloppes et scripts des
musiques), `menu.ff6data` (police et palettes des menus, main du curseur,
portraits) et une carte par fichier dans `maps/` ; il n'ecrase jamais un
dossier existant. `--text` utilise un `dialogues.json` modifie. Un caractere
absent de la police de la ROM, par exemple un accent, provoque une erreur
explicite. Le pack contient des donnees du jeu : ne pas le distribuer. Le format
est versionne (version 2 : ajout des mathematiques de couleur) ; un ancien pack
doit etre regenere.

Controles :

| Action | Clavier | Manette |
| --- | --- | --- |
| Deplacement | Fleches / WASD | Croix ou stick gauche |
| Parler / valider | Entree / Espace | A (bouton du bas) |
| Menu | X | X |
| Annuler | Z / Retour arriere | B |
| Echelle entiere | F10 | |
| Sauvegarder | F5 | |
| Plein ecran | F11 / Alt+Entree | |
| Quitter | Echap | |

La fenetre est redimensionnable. L'image 256 x 224 est agrandie au plus proche
avec des bandes noires. Les manettes reconnues par SDL (Xbox, PlayStation,
Switch Pro, etc.) peuvent etre branchees a chaud.

`--capture image.bmp [--at x,y] [--wait images] [--moves UUDDLLRR] [--talk images]`
produit une capture sans afficher de fenetre : le groupe est place sur la case
`x,y`, `images` images passent (les PNJ marchent), les deplacements sont
joues, puis la touche de validation est pressee (ce qui parle a un PNJ en face)
et `images` images sont simulees avant la capture.

Limites actuelles :

- Decor : pas d'animation de palette, pas de cartes du monde (0 a 2). La
  transparence (mathematiques de couleur : addition, soustraction, moitie)
  est reproduite pour bg1/bg2/bg3 et les sprites, avec la couleur fixe a zero.
  Les portes s'ouvrent quand le groupe marche vers elles (haut ou bas), avec
  leur bruit ; une porte ouverte redevient fermee si la carte est rechargee.
- PNJ : ils errent ou restent en place selon leurs donnees, et leurs scripts
  d'objets (marche, pauses, sauts, tours, apparition, interrupteurs) sont
  executes, y compris pendant les evenements. Vehicules, PNJ a graphismes
  speciaux et animations speciales sont ignores ; le hasard n'est pas celui de
  la console.
- Evenements : sont executes les dialogues, les choix, les interrupteurs, les
  conditions, les boucles, les appels, les pauses, les scripts de tous les
  objets (PNJ, personnages, camera, places du groupe) avec l'attente de leur
  fin, les changements de carte (l'evenement continue une fois la carte
  chargee), le groupe (composition, activation, carte,
  position), le verrouillage de l'ecran, les palettes de sprites et les
  modifications de tuiles du decor (portes ouvertes par un evenement) et les
  fondus (entrees et sorties de carte, commandes 0x59 a 0x5C, 0x96 et 0x97 ;
  l'ecran est noir tant que la carte n'a pas fini de se charger). Un script
  d'objet absent (desactive, ou personnage sur une autre carte) est saute ;
  une attente d'objet est abandonnee apres 30 secondes. Les autres commandes
  connues sont ignorees (argent) et une
  commande inconnue termine l'evenement avec un message sur la sortie d'erreur.
- Personnages : le chef du groupe est un objet comme les autres, pilote par
  le joueur hors evenement et par script pendant un evenement ; il retrouve
  ensuite le mouvement prevu (commandes 0x3A et 0x3B). Seule Terra existe au
  depart, sauf avec `--party terra,locke,edgar,sabin` (jusqu'a quatre
  personnages, par nom ou par numero) : les objets des places du groupe existent
  alors et seul le chef est visible. La palette par defaut de chaque personnage
  est celle que les evenements du jeu lui donnent quand il rejoint le groupe.
  L'etat de sauvegarde par defaut est reconstruit par le moteur ; le
  deplacement de groupe (0x7E) place le groupe, noircit l'ecran et le
  rallume sans recharger la carte : PNJ, portes et tuiles restent comme ils sont.
- Sauvegarde : le moteur a son propre format, un fichier JSON (pas de SRAM
  emulee). F5 ecrit `ff6-save.json` (ou le fichier de `--save`) et
  `--load fichier` reprend la partie : carte, case et direction du groupe,
  groupe actif, chaque personnage (present, visible, groupe, place, carte,
  graphisme, palette, case, direction), bits d'evenements et de combat,
  palettes de sprites et carte parente des maisons. Le fichier est versionne
  (`version`) et verifie a la lecture : une valeur qui n'existe pas donne une
  erreur claire. L'evenement de demarrage de la carte s'execute au
  chargement, comme dans le jeu ; le morceau en cours, l'etat des PNJ et les
  portes ouvertes ne sont pas sauves.
- Menus : X ouvre le menu principal (l'ecran se fond en noir, puis le menu
  apparait) ; Z ou X le referment. Il reproduit le menu du
  terrain : fenetres, groupe avec portraits, liste Item a Save, temps, pas et
  Gp, main du curseur (haut/bas, avec la repetition de la console). L'option
  Config ouvre ses deux pages (qui defilent l'une sous l'autre quand la main
  sort de la premiere ou de la seconde) : mode, vitesses, commandes, jauge,
  son, curseur, reequipement, manettes ; ordre des magies, motif et couleurs
  des fenetres (8 motifs de la ROM, 7 couleurs modifiables chacun), couleur
  de la police (la validation les remet a zero). Gauche/droite changent un
  reglage, haut et bas deplacent la main sans boucler, Z revient. Les
  reglages sont ceux de la console (octets $1d4d, $1d4e, $1d54, couleurs),
  sauvegardes dans le JSON ; les couleurs et le motif des fenetres agissent
  sur les menus et sur les dialogues, et « Cursor : Memory » sur le menu
  principal. Les vitesses de message et de combat ne servent qu'aux combats
  (le dialogue du terrain n'y est pas sensible dans le jeu) et le mono
  demande la commande 0xF3 du son, pas encore portee. L'option Item ouvre la
  liste de l'inventaire (noms de la ROM, quantites, defilement) avec les
  options Use, Arrange et Rare ; l'inventaire est un inventaire de
  remplacement, les descriptions ne sont pas extraites et les objets ne
  peuvent pas encore etre utilises. Les autres options ne sont pas encore
  branchees (le son d'erreur est joue), et les statistiques des
  personnages, l'argent et les pas sont des valeurs de remplacement.
  `--capture ... --menu n [--select k] [--down k]` capture le menu avec le
  curseur sur l'option n, en appuyant k fois sur la validation puis k fois
  sur bas.
- Audio : le pack contient les 63 echantillons et les 85 musiques. Le moteur
  decode les echantillons, interprete les scripts de musique (8 voix, tempo,
  volumes, panoramique, vibrato, tremolo, glissando, boucles, liaisons, echo,
  bruit, modulation de hauteur) et les joue sur 32 kHz stereo avec
  l'interpolation gaussienne de la puce sonore. Le jeu choisit la musique :
  l'evenement de demarrage de la carte (lu dans le pack) s'execute a chaque
  entree par une porte, et les commandes d'evenement de musique sont
  interpretees (lecture, volume, fondus, suspension et reprise d'un morceau,
  commande 0xFC du retard d'echo, attentes de fin ou de position du morceau).
  `--music 22` remplace le morceau de la carte. Les effets sonores du jeu
  (commandes F4 et F5, 256 effets) et du systeme (curseur, erreur, etc.) sont
  joues par des scripts sur les voix 4 a 7, qui les prennent a la musique,
  avec leur pan et leur volume. Le tempo et la hauteur du morceau changent
  sur commande du jeu (0x84 et 0x85) et restent d'un morceau a l'autre. Un
  glissement du volume des effets jusqu'a zero les arrete. Un morceau peut
  lancer un effet sur sa voix (commandes E9 et EA). Non pris en charge : mode
  mono. Les attentes du son sont abandonnees
  au bout d'une minute. Le retard d'echo vaut 80 ms au
  depart, comme dans le jeu ; `Synth::setEchoDelay` le change (0 a 15 pas de
  16 ms) et `ff6_audio --echo n` l'essaie.
  `build/engine/ff6_audio pack --song n --seconds 30 --wav out.wav` ecrit un
  morceau dans un fichier WAV, `--sample n [--pitch p] [--hold s]` un
  echantillon seul, et `--sfx n [--pan p]` ou `--system n` un effet sonore.
- Pas de combats, de menus, d'escaliers diagonaux ni de remappage
  des touches ; pas de graphismes HD.
- La compilation n'a ete validee que sur macOS arm64 (Apple clang, SDL2 2.32).

Organisation du moteur (`engine/src/`), un fichier par fichier du
desassemblage de reference (`src/field`, `src/event`, `src/data`) :

| Dossier | Fichiers | Role |
| --- | --- | --- |
| `data/` | `pack` | lecture du pack (donnees communes), conversion des couleurs |
| `field/` | `map`, `bg`, `anim`, `scroll`, `color` | carte, rendu de bg1/bg2/bg3, animations, defilement et parallaxe, composition avec transparence |
| `field/` | `player`, `obj`, `obj_party`, `obj_script`, `sprite_data` | deplacements du groupe, objets (PNJ, personnages, camera, groupe) et leurs scripts, images des sprites |
| `field/` | `text`, `entrance`, `event`, `event_cmd` | dialogues, entrees et sorties, interprete d'evenements et taille des commandes |
| `audio/` | `audio_data`, `brr`, `dsp_voice`, `akao` (+ `akao_script`, `akao_tables`), `synth`, `sound_program` | donnees audio du pack, decodage BRR, voix de la puce sonore, sequenceur de musique, mixage et echo, commandes du jeu au programme sonore |
| racine | `game`, `renderer.hpp`, `sound.hpp`, `input.hpp`, `constants.hpp` | jeu et son dessin, interfaces, macros `FF6_*` |
| `sdl/`, `main.cpp` | | backend SDL2 (fenetre, rendu, clavier et manettes) |

Les outils Node suivent la meme organisation (`tools/field/`, `tools/event/`, `tools/sound/`,
`tools/data/`, `tools/common/`) et `tools/pack.mjs` assemble le pack. Les
tests du moteur ont un fichier par fichier source dans `engine/tests/`.

Seuls `sdl/` et `main.cpp` dependent de SDL2. Le jeu dessine uniquement via
l'interface abstraite `ff6::Renderer` (textures RGBA, copie de rectangles) et
recoit ses commandes sous forme de `ff6::InputState`. Le son passe de meme par
`ff6::Sound` (commandes du programme sonore et ports de retour). Pour utiliser une autre
bibliotheque graphique, implementer `Renderer`, fournir un `InputState` a chaque
tick et remplacer `sdl/` et `main.cpp`.

## Tests

Tests unitaires sans ressources commerciales :

```sh
node ff6.mjs test
```

Tous les tests, avec validation et extraction de la ROM personnelle dans des
dossiers temporaires automatiquement nettoyes :

```sh
node ff6.mjs test --rom "Final Fantasy 6 [US].smc"
```

Sans ROM, les tests d'integration sont explicitement ignores. Une ROM fournie
mais incompatible fait echouer les tests ; elle n'est pas silencieusement ignoree.
La variable d'environnement `FF6_ROM` est aussi acceptee par les tests.
Les tests du moteur (`ctest`) utilisent `FF6_PACK`, le dossier d'un pack
genere par `build-pack`, pour leurs tests sur les vraies donnees.

## Perimetre restant

- Reconstruction binaire de la ROM via le desassemblage de reference.
- Cartes du monde, animation de palette et autres textes.
- Commandes d'evenements restantes (defilements, changements de decor
  anime, eclairs, tremblements, filtres de couleur, mosaique).
- Portage natif des menus et combats ; sauvegarde du jeu complet (argent,
  objets, statistiques).
- Audio natif sans emulation du SPC700 : mode mono.
- Interface adaptable aux ecrans larges et remappage des commandes.
- Gestion des graphismes HD.
- Validation du moteur sous Windows.

Les outils ont ete executes sur macOS avec Node.js 25.8.1. Le code n'utilise
pas de commande shell propre a macOS, mais son execution Windows et Node.js 24
n'a pas encore ete testee ici. Les tests de liens symboliques sont ignores
sur Windows pour ne pas exiger de privileges supplementaires.

## References et distribution

References techniques consultees dans
[FF6 Disassembly](https://github.com/everything8215/ff6), revision
`813013276c952fdd27edcf7b8b86f17291542cbf` :

- [Regions de donnees et palettes](https://github.com/everything8215/ff6/blob/813013276c952fdd27edcf7b8b86f17291542cbf/tools/rip_list_en.json).
- [Caracteres des dialogues](https://github.com/everything8215/ff6/blob/813013276c952fdd27edcf7b8b86f17291542cbf/tools/char_table/dialog_en.json).
- [Commandes des dialogues](https://github.com/everything8215/ff6/blob/813013276c952fdd27edcf7b8b86f17291542cbf/tools/char_table/dialog_escape.json).

La disposition des portraits est documentee par la definition
`portraitGraphics.assembly.spriteSheet.tilemap` de
[FF6Tools](https://github.com/everything8215/ff6tools/blob/dca0e8ce5e72eb6b34f977ccf88f591f6e8aca91/ff6/ff3u-sfc.json),
revision `dca0e8ce5e72eb6b34f977ccf88f591f6e8aca91`.

Les projets de reference sont sous GPL-3.0 ; toute reprise de leur code doit respecter
leurs conditions. Ces outils n'embarquent ni le moteur ni les ressources du jeu.
Les textes, images, tuiles et palettes sont produits uniquement depuis la ROM
locale, sans envoi reseau. Ne pas les inclure dans la distribution des outils.

La ROM et `output/` sont ignores par Git. Une sortie choisie ailleurs n'est pas
automatiquement ignoree ; verifier les fichiers avant toute publication.