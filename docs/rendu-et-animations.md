# Rendu des combats et animations d'attaque

Ce document décrit comment le moteur dessine un combat et comment il joue les
animations d'attaque du jeu (sorts, techniques, coups d'arme, outils). Il dit
aussi ce qui n'est pas fait et comment le vérifier. Les cartes, les menus et
les dialogues sont décrits dans le [README](../README.md).

Tout vient de la ROM américaine 1.1. Pour lire le code du jeu, la référence est
le désassemblage `everything8215/ff6`, commit
`813013276c952fdd27edcf7b8b86f17291542cbf` (`src/btlgfx/*.asm`).

## 1. Du ROM à l'écran

```
ROM ──tools/battle/attack_anim.mjs──> output/pack/attackanim.ff6data
                                      (scripts, images, palettes, tables)
        engine/src/data/attack_anim.cpp        charge le fichier, décode les images
        engine/src/battle/attack_anim_player   exécute les scripts, image par image
        engine/src/battle/battle_screen.cpp    dessine, par-dessus le combat
```

- **Pack `attackanim.ff6data`** (en-tête `FF6ATK01` + version) : propriétés des
  406 animations (14 octets chacune), propriétés des 650 images/scripts
  (6 octets), pointeurs de scripts, scripts (0x7FB2 octets), pointeurs et
  données des cadres, tuiles 3 bpp et 2 bpp, palettes, graphismes, et les 128
  propriétés d'animation d'arme (8 octets). Les adresses ROM sont dans
  `tools/battle/attack_anim.mjs`.
- **Une animation** (14 octets, `AttackAnimProp`) : script de sprites, script de
  bg1, script de bg3 (`0xFFFF` = aucun ; le bit `0x8000` = graphismes seuls),
  trois palettes, effet sonore, fonction d'initialisation, script
  supplémentaire, délai entre deux cibles.
- **Un cadre** : une liste de cellules de 16×16 pixels prises dans une planche,
  avec leurs retournements. `AttackAnim::frame(script, cadre, palette, miroir,
  rotation)` le décode une fois (cache) en image RGBA ; l'index 0 de la palette
  est transparent.

### Indices de la table des animations

| Utilisation | Indice |
|---|---|
| Sort ou attaque | son numéro d'attaque |
| Outil (Tools) | 328 + (objet − 163) ; Chain Saw qui tue : 336 |
| Bio Blaster, Flash | attaques 125 et 126 |
| Coup d'arme | table à part (`weapon(objet)`), un script par main |

## 2. Composition de l'écran de combat

`BattleScreen::draw` peint, dans cet ordre (le champ fait 256×152 pixels, les
fenêtres du bas, hautes de 8 rangées (de la ligne 152 à la 216), sont dessinées après) :

1. **Le fond** (`drawField`), avec les ondulations des fonds « chauds », éclairci
   ou assombri par les teintes de palette du groupe « champ ».
   Une copie va dans `bgPixels_`. Le fond fait 19 rangées (152 lignes) : la zone
   des fenêtres commence à la ligne 152 (dans le jeu, le HDMA de bg2 y change
   dès la ligne 151).
2. **Les monstres puis les personnages**, avec leurs teintes, leurs poses, leurs
   déplacements et masquages demandés par un script ; ils vont dans
   `objectPixels_` (RGBA avec transparence), puis sur l'écran.
3. **L'éclair** de couleur d'un élément (`flashTimer_`).
4. **L'animation** (`drawAnimation`) : l'esprit au milieu du champ, puis les
   couches dans l'ordre bg3, bg1, sprites. Pour chaque couche : teinte,
   rotation de palette, ondes, fenêtre, mélange (§5).
5. **Les effets dessinés à la main** (`drawFx` : trait, anneau, étincelles) pour
   les attaques sans animation, puis les nombres de dégâts.

En mode 7 (§6 bis) la composition change sur les 151 lignes du champ : ni fond, ni bg3 ; les fenêtres du bas restent.

Correspondance avec les couches de la SNES, déduite des registres des fonctions
d'initialisation :

| SNES | Dans le moteur |
|---|---|
| BG2 | le fond de combat (`bgPixels_`) |
| OBJ (sprites) | monstres, personnages, et les cadres de la couche 0 |
| BG1 | la couche 1 de l'animation (script de bg1) |
| BG3 | la couche 2 de l'animation (script de bg3) |

## 3. L'interpréteur de scripts

`AttackAnimPlayer` joue les scripts comme le jeu :

- **Un fil par cible**, plus des **copies** décalées dans le temps (table
  `kCopies` : type d'initialisation → copies et délai). Les scripts de bg1 et de
  bg3 sont des fils de plus sur la première cible. Un fil avance d'un octet de
  script par image ; `rate` est le nombre d'images par cadre.
- **En-tête de script** : l'octet 0 donne la vitesse (quartet haut + 1), l'octet
  1 l'alignement (`0x00` bas du corps, `0x40`, `0x80`/`0xC0` devant, `0xA0`
  écran, `0xE0` centre) qui place le fil sur chaque cible et sur l'attaquant.
- **Cadres** : les octets `< 0x80` sont des numéros de cadre (`0x1F` = vide).
  Les commandes sont `0x80..0xFE` ; `0xFF` termine le fil. Les longueurs sont
  dans `commandLength` et `specialLength`.
- **Les sons** : `takeSounds()` rend les effets demandés depuis le dernier appel.

### Commandes gérées

| Code | Rôle |
|---|---|
| `$80/..` | `01`,`66` efface le décalage ; `10` va à la cible ; `4F` à l'attaquant ; `2E` à une position absolue ; `58` forme du cercle ; `57` fenêtres de bg3 ; `86`,`87`,`8C` son ; `2D` saute selon le type de combat ; `32` selon le sens ; `72` saute si le coup n'a pas manqué ; `3E` couches de l'écran principal ; `40`, `41`, `42`, `5E` mode 7 (§6 bis) |
| `$81`,`$82` | pose du personnage attaquant ou ciblé, selon le sens |
| `$83` | déplace le fil (`dddxxxxx`) |
| `$84` | change la vitesse |
| `$85`,`$91` | va à la position de l'attaquant |
| `$86`,`$87` | déplace l'attaquant ou la cible (décalage à l'écran) |
| `$89`,`$8A`,`$8B`,`$8C`,`$9F` | boucles ; `$8B`/`$8C` numérotent les cadres ; `$9F` boucle autant que de fils |
| `$92`,`$93`,`$94`,`$95`,`$E7` | vecteurs de l'attaquant à la cible (`$94` : vers un point au hasard de la cible) |
| `$98` | le décalage de cadre compte tout seul |
| `$9A`,`$A2` | sens du fil |
| `$A3`,`$A4` | rotation de palette (sprites, bg1, bg3) |
| `$A5`..`$A9`, `$C3`, `$D3` | cercle de fenêtre (§6) |
| `$AA`..`$B9`, `$BA`..`$BC`, `$CC`..`$CF`, `$FB`..`$FE` | teintes de palette (§5) |
| `$AC`,`$AD`,`$AE` | ondes de défilement de bg1 et bg3 (§5) |
| `$BD` | fige ou libère les graphismes de bg1 et de bg3 (§7) |
| `$BF`,`$C0` | appel et retour de sous-script |
| `$C4` | le fond de bg suit la position de ce fil ; pour bg1 il fixe aussi le point de départ du défilement (§6 bis) |
| `$C9` | son |
| `$D2` | position de la cible du fil |
| `$D4` | mathématiques de couleur (§5) |
| `$E1`,`$F1` | cache ou montre l'attaquant ou la cible |
| `$E8`,`$EF` | mouvement polaire (rayon et angle), normal ou aplati |
| `$EB`,`$F0`,`$F3`,`$FA` | sauts (par indice de copie, par cible, par attaquant, fixes) |
| `$F8` | saut selon le mode Magitek (`setMagitek`) : l'adresse normale, ou celle de l'armure |
| `$EC` | le fil passe dans une autre couche |
| `$D0` | priorité des combattants (octet >> 4) face à bg1 en mode 7 |
| `$90`,`$D1`,`$F7` | reconnus, sans effet dans ce rendu (ordre des sprites, priorité des tuiles, attente de ligne) |
| `$C2` | relâche les fils en attente de bg1, bg3 ou des sprites : l'init `0x4E` (Fire, Ice et Bolt Beam, Tek Laser) met les sprites et bg3 en attente, pour que la lueur et la flamme ne viennent qu'après la marche de l'armure |
| `$8D`,`$8F`,`$9C` | déplace le fil si l'attaquant (ou la cible) regarde dans tel sens |
| `$8E`,`$9B`,`$F4` | le fil passe derrière ou devant les combattants (`AnimSprite::back`, noté mais pas encore dessiné) |
| `$DB` | saute le pas en arrière d'un personnage ; un monstre ne le fait jamais |
| `$F2` | vecteur de la cible à l'attaquant |
| `$A0`,`$A1` | saut de l'attaquant le long du vecteur, aller ou retour, avec un arc de 32 pixels (`AnimShift::vx`, `vy`) |
| `$ED` | vol vers la cible en tournant le cap petit à petit (la marge grandit à l'usage) ; `$80/$0F` en fixe le cap de départ |
| `$C1` | avance par pas le long du vecteur, puis le fil se pose là et le vecteur tourne de `0x18` |
| `$D5` | retourne le monstre ciblé (`AnimShift::flipX` et `flipY`, dessinés) |
| `$D6` | défile le champ (bg2) ; `$D7` : sprites de la danse du feu ; `$BE` : mosaïque (`mosaic()` : taille en quartet haut, bg1 = bit 0, bg2 = bit 1, bg3 = bit 2 ; chaque bloc prend la couleur de son premier pixel, `MenuScreen::mosaic` pour le champ) |
| `$D9`,`$EE`, `$80/$28` | priorité de tuile des combattants (`AnimShift::priority` ; 3 = devant bg1, redessiné après bg1) |
| `$DD`,`$DC`,`$F6`,`$DE`,`$DF`,`$E0`, `$80/$4C`,`$4D` | le triangle : le jeu en fait la forme de la fenêtre 2 (même mécanisme que le cercle) avec trois sommets sur un cercle (une ellipse pour la 3D) |
| `$EA`, `$80/$26`,`$29`,`$46`,`$47` | n'agissent que sur la mémoire vidéo, les curseurs et les palettes propres du jeu : rien à dessiner |
| `$80/$04`,`$0C`,`$11`,`$12`,`$16`,`$1D`,`$52`,`$21`,`$31`,`$63`,`$5D`,`$44`,`$45`,`$4E`,`$1A`,`$48`,`$1C`,`$24`,`$25` | mouvements du fil (angle et position polaires au hasard, première position du groupe, orbites de rayon 32 et 16, ondes sinusoïdales, copie du fil précédent), registre de fenêtre, montre ou cache un monstre, assombrit l'écran (`dimmed()` : un seizième de noir par pas, tout l'écran), efface les ondes de bg3 et de bg1 |
| `$E2`,`$E3`,`$E4` | comme `$A0`, `$A1` et `$E7` pour un monstre attaquant |
| `$80/$3B`,`$3C` | la cible prend les couleurs de l'animation (palette 3 du jeu), puis les rend : un monstre est redécodé avec les 8 premières couleurs de la palette de l'animation (`BattleGfx::monsterWith`), un personnage avec `characterWith` |
| `$80/$43`,`$55`,`$50`,`$56`,`$76`,`$5A`..`$5C` | défilement par ligne de bg1 ou de bg3 (`AnimLineScroll`, 151 lignes) : chanson de la lune, quasar, vents au hasard, tentacules qui saisissent, souffle mental ; le jeu lit sa table de nombres au hasard (`RNGTbl`, `C0/FD00`, dans `attackanim.ff6data`) : un mot par ligne pour `$50`/`$56`, un octet sur quatre pour `$5B` |
| `$80/$59` | fenêtre 2 ouverte entre deux bords (flare star, goner) |
| `$80/$61`,`$62`,`$75` | rebonds sinusoïdaux du fil (pluie de chats, balle super) ; `$80/$65`, `$49` : palette de sprites au hasard (arc-en-ciel, encre) |
| `$80/$6A`,`$6B`,`$6C` | aligne le fil sur le bas ou le centre de la cible ; vide les cadres de bg1 et bg3 |
| `$80/$13`,`$14`,`$77`,`$78`,`$7C`,`$7E`,`$7F`,`$80`,`$82`,`$83`,`$84`,`$8A` | personnage en diablotin ou invisible, personnage saisi emmené chez l'attaquant, échange cible et attaquant, tous les monstres cachés, fondu des monstres, tremblement, entrée de Chadarnook, priorité d'un monstre |
| `$80/$1E`,`$1F`,`$20`,`$22`,`$23` | la perle : défilement vertical de bg3 ou de bg1 par pas de la table `_c2d39f` (54 durées, d'abord vers l'avant, puis à rebours), et défilement horizontal de bg3 par le bord gauche d'un cercle de 76 pixels |
| `$DA`, `$80/$06`..`$09` | la tornade : le fil tourne autour de sa place, de plus en plus large (angles `$10` et `0`, amplitude jusqu'à `$40`) ; `$06` pose dans le tableau de défilement de bg1 une sinusoïde de 8 pixels sur 64 lignes (`_c10d8b`), `$DA` en recopie une fenêtre (décalée de 2 lignes par image) sur les lignes 0 à 63 |
| `$80/$79`,`$7A`,`$7B`,`$53`,`$54`,`$60` | les personnages courent le long de leur ligne (angle de 4 en 4, `AnimShift::angle`) puis se retournent, passent à l'autre rangée (pas signés de la table `C2/A86F` (vers l'arrière) et `C2/A87F` (vers l'avant), indexée par type de combat × 4 + place, 6 pas ; `AnimShift::rowAngle`), états de l'attaquant ajoutés ou retirés pour le dessin (`statusXor`) |
| `$CB` | échos d'un personnage : jusqu'à huit positions passées gardées tous les `masque + 1` images, quatre dessinées à chaque image, une sur deux (`AnimEcho`, `BattleScreen::echoHistory_`) |
| `$80/$73`,`$1B` | la face d'un dé (`setDice`) ; un personnage qui suit un vecteur n'emporte plus le fil |
| `$80/$85` | lumières de Chadarnook : le champ prend la palette du jeu, allumée ou éteinte (`attackanim.ff6data`, 2 × 96 octets en `C2/E288`) |
| `$99` | la rangée de palette d'un fil (le Crusader l'a déjà par sa feuille d'esprit) : sans effet ici |
| `$F9` | l'action de l'armure Magitek de l'attaquant ou de la cible (0 debout, 1 marche, 2 tire : `AnimShift::armor`) ; `$E7` vise 24 pixels plus haut en mode Magitek |
| `$80/$3F` | attend, en rejouant le cadre d'avant, que le fil de bg1 arrive à la hauteur de l'attaquant (saut de Sonic Dive) |
| `$80/$02`,`$03`,`$0E`,`$18`,`$19`,`$34`..`$36`,`$4A`,`$51`,`$64`,`$67`,`$68`,`$6D`,`$6F`,`$70`,`$71`,`$89` | registres, palettes et compteurs du matériel, sans équivalent ici |

`AttackAnimPlayer::skipped()` donne les commandes sautées et leur nombre : c'est
l'outil pour trouver ce qu'il reste à faire. Le test du lecteur
(`attack_anim_player_tests.cpp`) joue les 406 animations et vérifie qu'aucune
commande de la liste ci-dessus n'est sautée ; `FF6_SKIP_PROBE=1 FF6_PACK=... ff6_tests`
affiche la liste de celles qui le sont encore, avec quelques attaques.

## 4. Fonctions d'initialisation

`battle/attack_anim_inits.hpp` (`kAnimInits`) contient, pour environ cent
fonctions d'initialisation du jeu, ce qu'elles règlent en dehors des fils :

| Colonne | Registre de la SNES | Sens |
|---|---|---|
| `w12` | `W12SEL` | fenêtres de bg1 (quartet bas) |
| `w34` | `W34SEL` | fenêtres de bg3 (quartet bas) |
| `shape` | `wCircleShape` | forme du cercle (§6) |
| `cgwsel` | `CGWSEL` | bit 1 : mathématiques avec le sous-écran |
| `cgadsub` | `CGADSUB` | couches concernées, ajout ou retrait, moitié |
| `subscreen` | `TS` | couches du sous-écran qui sont ajoutées |

La table vient de la lecture du code de `src/btlgfx/anim_init.asm` (appels à
`SetColorMathHDMA`, écritures de `hW12SEL`, `hW34SEL`, `wCircleShape`) ; un
script jetable la produit, elle est ensuite maintenue à la main.

## 5. Couleurs, mélange, ondes

- **Teintes** (`AnimTint`, six groupes : bg3, champ, bg1, sprites, monstres,
  personnages). Chaque commande donne des valeurs 0 à 31 par composante, soit
  absolues, soit relatives à la dernière valeur ; la commande dit si elles sont
  ajoutées ou retranchées. L'écran les applique à l'image (valeur × 8).
- **Mélange** (`AnimBlend`). Quand une fonction d'initialisation ou `$D4` règle
  le sous-écran, les couches nommées par `cgadsub` ne se posent plus en opaque :
  le pixel de la couche remplace celui de la scène et reçoit en plus ce que le
  sous-écran montre au même endroit (`ts` : le champ pour `0x02`, bg3 pour
  `0x04`, bg1 pour `0x01`, les monstres et personnages pour `0x10`, la couche de
  devant l'emportant), ajouté ou retranché. Pour une addition par moitié (bit
  `0x40`), la moitié ne s'applique que là où le sous-écran a un pixel ; sans
  pixel c'est le fond du sous-écran et la couche garde sa pleine valeur, comme sur
  le matériel. C'est ce qui donne la lueur du Wave Cannon (la scène s'éclaircit
  sous le calque), le ciel étoilé du Météo (le calque couvre le fond, les monstres
  restent visibles) et le dôme bleu plein d'Ultima (les monstres y sont délavés,
  les météorites de bg3 plus sombres). Les couches de fond sont dessinées chacune
  dans un tampon à part (`layerPixels_`) pour que ce mélange puisse les lire. Le
  mode « couleur fixe » (`CGWSEL` bit 1 à 0) n'est pas fait.
- **Ondes** (`AnimWave`) : `$AC` donne l'amplitude et la vitesse, `$AE` fait un
  pas. L'image du calque est décalée ligne par ligne par un sinus. L'amplitude en
  pixels est estimée (je n'ai pas lu les tables de sinus du jeu).
- **Bouclage** : un calque de fond qui défile revient de l'autre côté de l'écran.
- **Rotation de palette** (`AnimCycle`) : une plage de couleurs tourne ; le
  décodage des cadres reçoit l'état de rotation et le met en cache.

## 6. Fenêtres et cercles

Sur la SNES, un cercle est une fenêtre : une table, ligne par ligne, du premier
et du dernier pixel visibles. Le moteur garde :

- `circle_` : centre, taille, taille finale (`$A5` à `$A9`, `$C3`, `$D3`).
- `spanLeft_`, `spanRight_` : la fenêtre 2 (le cercle), recalculée par `$A7`
  (`updateCircle`). La fenêtre 1 est vide. Les bornes sont 8 et 247, comme dans
  le jeu.
- Formes faites : cercle, ovale horizontal (moitié moins de lignes), ovale
  vertical (deux fois plus), rayon du haut (demi-cercle bas et ligne la plus
  large jusqu'en haut), dôme d'Ultima (moitié haute complète, moitié basse
  aplatie de moitié), Bio Blast et les trois « blobs ». Le Bio Blast décale le
  centre de chaque ligne par un sinus (amplitude = vitesse donnée par `$A5`, angle
  qui avance de 2 à chaque mise à jour) ; les blobs font aussi pulser le rayon,
  et décalent chaque ligne de la table du jeu (les lignes au-dessus du centre
  comptent à rebours depuis 127, celles du dessous depuis 128) par un sinus dont
  le pas, l'amplitude et l'avance dépendent du blob (Slimer 4, 0x10, 8 ; petit 8,
  0x0A, 4 ; grand 4, 0x20, 8). Le sinus est celui du jeu : −127 à 127 fois un
  multiplicateur, sur 256. Je n'ai pas comparé ces trois formes à des captures du
  jeu.
- `layerVisible(couche, x, y)` : pour chaque fenêtre activée du quartet de la
  couche, le masque est « dedans » inversé ou non ; la couche est cachée là où
  le masque est mis. Bit 0 : inversion de W1 ; bit 1 : W1 active ; bit 2 :
  inversion de W2 ; bit 3 : W2 active. Exemple : `0xC` = bg visible seulement
  dans le cercle.

## 6 bis. Le mode 7

Six attaques passent l'écran en mode 7 (commande `$80/$40` avec 7) : bg1 devient
un plan de 1024×1024 pixels qu'une matrice transforme.

| Attaque | Numéro | Fonction | Image du plan |
|---|---|---|---|
| Odin | 65 | `0x45` | l'esprit Odin (image d'esprit 11), dès la rangée de tuiles 1 |
| Raiden | 66 | `0x6B` | l'esprit Raiden (12), idem |
| Crusader (Purifier) | 69 | `0x74` | l'esprit 15 dès la rangée 6 ; quatre rangées de sa moitié droite sont déplacées sous la gauche (`_c1240a`) |
| H-Bomb | 128 | `0x75` | jeu de tuiles 2, carte 2 |
| S. Cross | 204 | `0x63` | jeu de tuiles 1, carte 0 |
| Overcast | 220 | `0x64` | jeu de tuiles 0, carte 1 |

- **Données** : l'outil décompresse `D8/D000` (trois jeux de 63 tuiles de 4 bits,
  espacés de `0x800`) et `D8/DAF2` (trois cartes de 32×32 numéros, espacées de
  `0x400`) dans `attackanim.ff6data` (version 10 du pack). Un numéro `63` est la
  tuile vide. `AttackAnim::mode7Plane` en fait l'image de 256×256 : le pixel `p`
  prend la couleur `p & 7` de la palette de bg1 (le jeu y recopie ses 8 couleurs
  deux fois par ligne). Le reste du plan est transparent ; le jeu y lirait ce
  que la VRAM contient d'autre.
- **Commandes** : `$80/$40` règle le mode (`b1 & 7`) ; `$80/$41` ajoute à
  `A`, `D` (zoom) puis au centre `X`, `Y` quatre octets signés (la matrice
  commence à l'identité, `0x100` = 1,0) ; `$80/$42` retourne l'écran (bit 0
  horizontal, bit 1 vertical) ; `$80/$5E` (Overcast) tourne : angle =
  −2 × compteur d'images, `A = D = sin × m`, `B = cos × m`, `C = −B`, avec
  `m` qui part de 255 et perd 2 à chaque usage ; `$80/$3E` choisit les couches
  de l'écran principal (bit 0 : bg1).
- **Défilement** : il vaut le point où est le fil de bg1 moins celui où `$C4`
  l'a mis (le jeu y retire une ligne, que le calcul compense).
- **Transformation** (`BattleScreen::mode7Pixels`), comme le matériel :
  `X = A·(x + H − X0) + B·(y + V − Y0) + 256·X0`, `Y = C·(x + H − X0) +
  D·(y + V − Y0) + 256·Y0`, le pixel du plan est `(X >> 8, Y >> 8)` modulo 1024.
  Les lignes comptent à partir de 1.
- **Écran** : le mode 7 n'a ni bg2 ni bg3, mais le jeu ne l'applique qu'aux 151
  lignes du champ : une table HDMA du registre de mode (`HDMA3Tbl`, 100 + 51 lignes
  pour le champ, 73 pour le menu) remet le mode 1 pour les fenêtres, qui restent
  donc visibles. Le fond de combat et l'esprit debout ne sont pas dessinés ; le
  fond est noir. Les monstres
  et personnages (OBJ) restent, devant bg1 si leur priorité (`$D0`) est 2 ou 3,
  derrière sinon. Les teintes de palette de bg1 s'appliquent au plan.
- **Approximations** : l'esprit debout est affiché là où le mode 7 montre d'abord
  son image (une copie de l'animation tourne jusqu'au passage en mode 7 pour le
  trouver), blanchi puis coloré en 50 images, et masqué pendant le mode 7 ; l'angle
  de `$5E` suit le compteur d'images de l'animation, pas celui du jeu.
- **Fil de l'esprit** : quand le champ « extra » des propriétés a le bit 15, le jeu
  crée un fil au nom de l'attaquant (Purifier en crée trois, qui se distinguent par
  `$EB`) ; sa planche est l'image de l'esprit (Odin, Raiden). Les trois figures de
  Crusader : celle du haut est l'image 15 (`CRUSADER_1`), celle de gauche l'image 29
  (les mêmes graphismes que la 28 avec la palette suivante, `$80/$67`), celle du bas
  l'image 28. Les deux figures de côté sont décalées de 8 pixels vers l'extérieur
  pour toucher les bords de l'écran (leurs scripts les font avancer de 8 pixels
  vers l'intérieur ; cause non établie).
- **Cercle et traits de Purifier** : le script de bg3 fait un cercle de 128 pixels
  dont le bord gauche reste à 128 pendant qu'il rétrécit, puis la forme Ultima
  (deux demi-ellipses, la basse moitié moins haute) qui grandit, puis des images
  de traits. L'image pleine (frame 8) et celles des traits font 256×160 pixels : le
  fil de bg3 est placé au centre de l'écran, ce qui couvre tout le champ et met le
  centre des traits à la tête de la figure de gauche (position tirée des captures
  du jeu, pas du code). Le dôme et les traits sont devant les figures, qui
  apparaissent à travers le dôme comme des formes plus claires : le jeu règle le
  mode avec le bit de haute priorité de bg3 (`$09`) et les tuiles de bg3 ont le bit
  de priorité, donc bg3 passe devant bg1 et les sprites pour toutes les attaques.

### Dégradés de couleur fixe

Le HDMA du jeu donne une couleur fixe à chaque ligne (151 lignes), que `$D4`
(bit 1 de `CGWSEL` à 0) ajoute ou retranche au fond (`0x20` de `CGADSUB`) et à bg2
(`0x02`) : le fond de combat, ou tout l'écran noir en mode 7. Commandes de `$80` :

| Sous-commande | Effet |
|---|---|
| `$3A` | fixe le niveau du dégradé |
| `$39` | ajoute à ce niveau, puis écrit un triangle bleu de 32 lignes (0 à 15 et retour) moins le niveau, répété le long de l'écran ; il avance d'une ligne à chaque usage |
| `$5F` | dégradé large (Overcast) : monte du haut et du bas vers le milieu |
| `$33`, `$4B` | arc-en-ciel et rouge/jaune (Megazerk) : la couleur change toutes les 32 lignes |
| `$37` | efface |

`BattleScreen::drawField` en fait une image de lignes qu'`imageMath` ajoute à ce
qui est déjà dessiné, avant les monstres. `$27` cache (1) ou montre (0) les
personnages, ce que fait toute attaque d'esprit.

## 7. Choses apprises en vérifiant

Les commentaires du désassemblage ne suffisent pas ; il faut lire le code qui
consomme la valeur, puis comparer avec une image du jeu.

- **`$BD`** : les commentaires disent « cacher » ; le code (`TfrAnimBGTiles`) ne
  fait que **ne pas recopier** les graphismes de la couche. La couche garde donc
  l'image qu'elle avait. Le moteur la fige (`showFrame`) au lieu de la cacher.
  Le Météo en dépend : son ciel étoilé est un cadre unique que bg3 garde.
- **`$D4`** : les trois octets sont `CGWSEL`, `CGADSUB`, `TS`, dans cet ordre,
  malgré le commentaire qui les décrit autrement.
- **Demi-addition** : la moitié ne s'applique que s'il y a un pixel au sous-écran.
  Sans cela Ultima est trop sombre (le bleu de bg1 est divisé par deux partout).
- **Animations d'outil** : l'indice du cadre est 328 + numéro d'outil, pas le
  numéro d'objet.
- **Feuilles d'esprit** : les fonctions d'initialisation d'invocation (`kEsperSheets`)
  chargent l'image de l'esprit à la place de la feuille de graphismes de bg1 ou des
  sprites ; les cadres la découpent par cellules de 16×16 et la rassemblent (la
  feuille de Bahamut est faite de morceaux). L'esprit ne se tient donc entier avant
  l'animation que pour les plans de mode 7.
- **Fonds qui défilent** : un cadre de 256 pixels de bg1 ou de bg3 remplit les quatre
  quarts de la carte et tourne autour de l'écran dans les deux sens, pas seulement
  à l'horizontale. Le défilement horizontal de bg1 vaut la base moins la position
  courante, la base étant d'abord la demi-taille du cadre et devenant la position du
  fil à `$C4 0x80` (`move_bg1_here`, `bg1Shift_`).
- **Priorité de bg3** : l'octet BGMODE vaut `$09` par défaut (mode 1, bg3 prioritaire).
  Les inits Justice (`0x3B`) et Possession (`0x77`) le retirent, et `$38`
  (`hi_priority_bg3`) le remet. Sans priorité, bg3 reste derrière bg2 et n'apparaît
  que par le mélange de couleurs.
- **Fil d'invocation** : les cadres du fil « extra » viennent de la feuille des
  sprites du script, pas de la feuille de bg3 ; sinon on obtient des formes parasites.
- **Fenêtre 2 de départ** : les inits Justice (128 lignes) et `0x5C` (72 lignes)
  appellent `_c2f178`, qui règle d'emblée la fenêtre 2 de `8` à `$F7`. Justice masque
  bg1 hors de cette fenêtre (`W12SEL = $3C`) : sans ces lignes, Alexandre
  restait invisible. Pour Justice le moteur l'ouvre sur toute la largeur, car la vidéo
  montre Alexandre jusqu'aux deux bords de l'écran (il était coupé à droite). Les 128
  lignes de la fenêtre expliquent aussi que l'image soit coupée net vers la ligne 128
  quand Alexandre monte : les sprites de l'animation (la bouche) sont coupés de même.
- **Palettes chargées par script** : `$80/$2A`, `$2B` et `$2C` remplacent la palette des
  sprites, de bg1 et de bg3. Justice charge la palette 118 à l'éclair ; sans elle les
  cadres de bg3 restent bleus et l'éclair n'illumine rien.
- **`move_bg3_here`** : comme pour bg1, la base du défilement devient la position du fil
  (`bg3Shift_`). Après l'éclair de Justice, les cadres de bg3 ont leur coin haut-gauche
  en (0, 0) et couvrent les 160 premières lignes ; les cadres 6 à 8 font 240 pixels de
  large, et le moteur étire leurs colonnes de bord pour couvrir tout l'écran, comme sur
  la vidéo.
- **Ordre des combattants et de bg1** : `_c1c3a7` de Justice met derrière bg1 les
  personnages dont le bas est au-dessus de la ligne `$81` (les trois premiers) et laisse
  le quatrième devant. Le moteur garde ces combattants dans un tampon à part
  (`behindPixels_`), que bg1 recouvre, et redessine les autres par-dessus bg1 ; le
  mélange de bg3 respecte le même ordre (champ, derrière, bg1, devant).
- **`$90` (priorité des sprites)** : la bouche d'Alexandre est faite de deux sprites : le
  cadre 0 est le cône (devant), le cadre 11 une forme sombre de priorité 0, c'est-à-dire
  sous le champ (bg2), donc jamais visible. Le moteur masque la priorité 0 pour Justice
  seulement ; sans cela on voyait un rectangle sombre pendant la teinte or et des blocs
  blancs pendant le flash final. Sur la vidéo, la visière du casque et la plaque d'armure
  du bas recouvrent le cône : le moteur dessine donc les sprites de Justice sous bg1 et
  laisse passer le sprite là où l'image est sombre (luma sous `FF6_BATTLE_POCKET_LUMA`),
  la poche d'ombre où se loge le cône. C'est une approximation d'une priorité par tuile
  que je n'ai pas retrouvée dans le code du jeu.
- **Phase des tremblements** : les fils de sprites commencent leur boucle (`UP 2, FORWARD 1`)
  à la première image, celui de bg1 à la deuxième ; ils tremblaient en opposition de phase,
  d'où une bouche qui glissait de côté pendant la montée. Le fil de bg1 de Justice part
  donc une image plus tard.
- **Voile de Justice** : mesuré sur la vidéo à décor identique (le combat 5 avec le fond 5),
  les flammes sont 17/16 plus larges que les cadres de bg3 autour du milieu de l'écran.
  Je n'ai pas trouvé la cause dans le code du jeu : c'est une correction mesurée,
  appliquée aux cadres 3 et suivants (`FF6_BATTLE_VEIL_*`). Verticalement, le voile
  coïncide avec la vidéo une fois celle-ci recalée sur le haut du HUD (ligne 152).
- **Image d'Alexandre (bg1)** : comme les flammes, elle est 17/16 plus large sur la vidéo,
  à partir de la colonne 121 (`FF6_BATTLE_BG1_PIVOT`, ajustée sur la vidéo) ; les sprites
  de l'animation suivent la même transformation de position, sans être élargis. Cela
  remplace le décalage de 5 pixels essayé d'abord.
- **Écarts mesurés qui restent** : sur la vidéo recalée, Alexandre est environ 7 pixels plus
  bas que dans le moteur ; le champ (bg2) paraît lui aussi environ 6 pixels plus bas, avec
  une bande noire en haut de l'écran (lignes 0 à 13) que le moteur ne dessine pas. Ces
  décalages verticaux ne sont pas corrigés.
- **Rangée arrière** : la position horizontale d'un personnage vient d'un angle
  (`$170` de base, `CalcCharXPos`) auquel la rangée arrière ajoute 12 ; le moteur
  ajoute maintenant `FF6_BATTLE_CHAR_ROW_ANGLE` pour `backRow`.

## 8. Comparaison avec le jeu

Captures d'un émulateur, rejouées avec `--play` :

| Attaque | Numéro | État |
|---|---|---|
| Wave Cannon | 183 | cercle, ondes, lueur : proche |
| Météo | 19 | ovale, ciel étoilé, monstres visibles : proche |
| Ultima | 20 | dôme bleu plein, monstres délavés, météorites sombres : proche |

Les cinq autres animations en mode 7 (§6 bis) n'ont pas été comparées à des
captures du jeu : elles donnent des images plausibles (Raiden, la gueule dorée du
Crusader, la bombe de H-Bomb, les anneaux bleus d'Overcast).

Odin (65) a été comparé à quatre captures : l'esprit blanc sur le champ puis
coloré sur le champ assombri, à la même place que dans le jeu (à quelques pixels) ; les
bandes de bleu, de même période (32 lignes) ; le zoom du mode 7 ; le groupe caché.
La vitesse de la montée du blanc à la couleur est devinée.

## 9. Outils pour travailler

```
./build/engine/ff6 output/pack --party terra,edgar,celes,strago --battle 5 \
    --play 183 --capture /tmp/x.png --after 60
```

- `--play n` joue l'animation n du premier monstre vers le groupe.
- `--capture x.png` ou `x.bmp` écrit une image 24 bits lisible partout ;
  `--after n` attend n images ; `--keys` envoie des touches.
- `--anim n --capture x.bmp` écrit la planche des cadres d'une attaque
  (`--layer 1` ou `2` pour ceux de bg1 ou de bg3).
- `--stock 1` donne armes, outils et Lores pour essayer leurs commandes.
- Tests : `engine/tests/battle/attack_anim_player_tests.cpp` (teintes,
  déplacements, fenêtre du Wave Cannon, mode 7), `tools/attack_anim.test.mjs` (pack).

## 10. Ce qu'il reste

Plus aucune commande n'est sautée : le test du lecteur joue les 406 animations et
exige que `skipped()` reste vide. Cela ne veut pas dire que chaque effet est dessiné
comme le jeu (le test ne compare aucune image). Ce qui est approché, noté sans être
dessiné, ou sans équivalent :

- **noté sans être dessiné** : la priorité des combattants plus fine que « devant bg1 »
  (0 à 2 reste sous bg1, comme sans priorité), la priorité des fils (`$90`).
  Les sprites `back` (`$8E`, `$9B`, `$F4`, `$80/$21`) passent sous les combattants, les
  combattants de priorité 3 par-dessus bg1, mais pas d'ordre par hauteur (`$80/$17`, `$69`) ;
- **sans effet ici** : les registres et palettes du matériel (liste du §3), la rangée de
  palette `$99` ;
- **approché** : la position verticale du triangle (centrée sur ses coordonnées,
  d'après `UpdateTriangle`) ; le saut de `$E2` et `$E3` d'un monstre (même arc que pour
  un personnage) ; l'ordre des échos de `$CB` ; le tableau de la tornade est posé tel que
  le code du jeu le lit (lignes 0 à 63 : la sinusoïde qui glisse de 2 lignes par image,
  lignes 64 à 127 : la sinusoïde fixe), sans comparaison à une capture ;
- **à comparer à des captures** : tout ce qui a été ajouté depuis la comparaison
  d'Alexandre, y compris les défilements par ligne, l'échange de palette de la cible
  (`$80/$3B`) et la perle ; formes de cercle (Bio Blast et blobs) ; mode « couleur fixe »
  des maths de couleur pour les calques (sprites, bg1 : seuls le fond et bg2 le
  reçoivent) ;
- les animations des objets consommables sont jouées d'après la table `ItemAnimPtrs`
  (l'objet `$E7` donne l'animation 402, `$E8` à `$FE` les animations 337 à 359).
