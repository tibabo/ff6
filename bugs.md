# Bugs : premier combat de Narshe

Écarts entre l'enregistrement du jeu sur Ares ([narshee-first-battle.mov](narshee-first-battle.mov)) et le moteur, sur le combat des deux gardes de Narshe (Terra, Wedge et Vicks en armures Magitek). Ce fichier est la liste des écarts ; une case cochée est corrigée. 

Conventions :

- **Jeu** = l'enregistrement ; **moteur** = le moteur natif.
- **image** = 1/60 s. **t** = numéro d'image de la vidéo depuis son début (arrondi de pts × 60) ; **f** = numéro d'image du moteur depuis le lancement d'une nouvelle partie, une image par `Game::tick`.
- Les durées sont mesurées image par image sur la vidéo ramenée à 256×224 ; ± 1 image.
- Une case à cocher par bug ; priorité : haute (déroulé), moyenne (menus), basse (cosmétique).

## Décisions à prendre avant de corriger

- [x] **Mode d'attente** : suivre le code du jeu ; le Wait ne fige que les sous-menus (B05).
- [x] **Rapport 8:7** : option, défaut carré (B33).
- [x] **Courbe de couleurs** : option, désactivée par défaut (B34).
- [x] **Hasard** : `RNGTbl` est portée (B03) ; l'index de départ dépend du temps de jeu, que le moteur ne compte pas comme la console.

## Chronologie de référence (jeu)

Toutes les valeurs sont des `t`.

| t | Événement |
|---|---|
| 1276–1306 | Mosaïque sur le champ |
| 1307–1410 | Écran noir |
| 1411–1447 | Fondu entrant du combat (fenêtres comprises) |
| 1454 | Les jauges repartent (19, 25, 26 sur 34 px pour Terra, Wedge, Vicks) |
| 1469–1476 | Les gardes entrent par la gauche |
| 1542 / 1557 / 1565 | Jauge de Vicks pleine / jaune / menu de Vicks ouvert |
| 1642–1645, 1650–1653 | Le garde du bas clignote (attaque 1) |
| 1705–1709 | Trait blanc sur la pile d'armures |
| 1737 / 1740 | Chiffre « 6 » / PV de Vicks 70 → 64 |
| 1755–1766 | Vicks valide Magi Tek (la liste défile) |
| ≈ 1800–1880 | Vicks cible (main sur le garde du haut) |
| 1866–1869, 1874–1877 | Le garde du haut clignote (attaque 2) |
| 1883 | Vicks a validé ; le menu de Wedge s'ouvre |
| 1961 | PV de Terra 63 → 56 |
| 2005–2068 | Encadré « Fire Beam » (Vicks) |
| 2126 | L'armure de Vicks quitte la pile |
| 2171–2201 / 2204–2225 | Rayon / boules de feu |
| 2282–2319 | Chiffre « 129 » ; dissolution violette de 2289 à 2301 |
| 2333 | Jauge de Vicks remise à zéro |
| ≈ 2128 | Wedge ouvre la liste Magi Tek ; la main est sur « Fire Beam » à t2160 |
| 2169–2220 | La main de Wedge est sur le garde du haut (il choisit la cible) pendant l'animation de Vicks |
| ≈ 2223 / 2261 | Wedge valide la cible, le menu se referme / le menu de Terra s'ouvre |
| 2393–2457 | Encadré « Fire Beam » (Wedge) |
| 2690–2727 | Chiffre « 127 » ; dissolution violette de 2696 à 2711 |
| 2740 | Jauge de Wedge remise à zéro |
| 2883–3086 | « Got 32 Exp. point(s) » |
| 3105–3175 | « VICKS gained a level » |
| 3194–3235 | « WEDGE gained a level » |
| 3254–3319 | « Got 96 GP » |
| 3328–3343 / 3343–3395 / 3396–3408 | Fondu sortant / noir / fondu entrant du champ |

## Déroulé du combat

### B01 — Transition d'entrée en combat (haute)
- [x] Jeu : mosaïque en dents de scie pendant 31 images (rampes qui retombent à zéro à t1283 et t1291, puis une dernière plus grande), coupure nette au noir, noir de 104 images, fondu entrant de 36 images. Total 171 images.
- Moteur (avant) : fondu sortant ≈ 15 images (f11161–f11175), fondu entrant ≈ 15 (f11176–f11190). ≈ 30 images, ni mosaïque ni noir.
- Cause : le moteur faisait un fondu de menu. Le jeu appelle `BattleMosaic` (`src/field/battle.asm`) : 32 images, taille `f & 7` puis `f & 15` (blocs de 1 à 8 pixels deux fois, puis de 1 à 16), sur les fonds seulement, champ figé ; puis le combat se charge écran éteint et `UpdateFadeIn` (`src/btlgfx/init_2.asm`) l'éclaire en 15 images en découvrant le champ depuis la ligne 76 en 38 images (couleur fixe retranchée par HDMA).
- Correctif : `Game::tickBattleEntry`, `battleMosaicSize`, `applyBattleFadeIn` ([game.cpp](engine/src/game.cpp)), mosaïque faite sur les couches de fond (les sprites restent nets, comme dans la vidéo), écran noir à luminosité 0 du combat, son `0xC1` et drapeaux « sans mosaïque » / « sans son » des commandes 0x4C/0x4D (constantes `FF6_BATTLE_MOSAIC_*`, `FF6_BATTLE_LOAD_FRAMES`, `FF6_BATTLE_FADE_*`). Moteur : mosaïque f11161–11191 (31 images, même grille de blocs que la vidéo), noir f11192–11296, fondu f11297–11333.
- Approché : les 104 images de noir sont le temps de chargement mesuré sur la console, pris constant pour tous les combats ; la musique du combat part toujours au lancement (le moment où le jeu la lance n'est pas vérifié, la vidéo n'a pas de son).
- Reste : le temps du combat repart à la fin du fondu (f11334), le jeu 7 images plus tard (B03) ; les gardes sont déjà là (B02).

### B02 — Entrée des gardes (haute)
- [x] Jeu : aucun monstre pendant le fondu ; ils entrent par la gauche 22 images après sa fin, en 7 images.
- Correctif : `introFrame_`, `FF6_BATTLE_ENTRY_*` (combat normal, monstres sans place imposée).
- Approché : mesuré, pas porté du script d'entrée du jeu (`EntryGfxScript`, `MONSTER_ENTRY_EXIT`), qui dépend du type d'entrée de la formation.

### B03 — Départ et valeurs initiales de l'ATB (basse)
- [x] Jeu : les jauges repartent 7 images après la fin du fondu ; leurs valeurs initiales viennent de la table de hasard.
- Correctif : `FF6_BATTLE_ATB_DELAY` ; la table du jeu (`RNGTbl`, C0/FD00) est portée dans `BattleRandom` (`Rand`, `RandA` : l'octet suivant de la table, multiplié et divisé par 256 pour un tirage borné), l'index part de l'octet bas du temps de jeu × 4 (`InitBattle`) et avance d'un cran à chaque tour de la boucle principale libre (`BattleLoop`).
- Approché : le moteur prend son compteur d'images pour le temps de jeu ; l'index de départ n'est donc pas celui de la console, et les tirages (jauges, dégâts, attaques des gardes) restent différents de la vidéo.

### B04 — Délai entre jauge pleine et ouverture du menu (moyenne)
- [x] Jeu : jauge pleine (blanche), jaune ≈ 15 images plus tard, menu 8 images après.
- Correctif : `readyTimer_` (`FF6_BATTLE_READY_FRAMES`), `menuDelay_` (`FF6_BATTLE_MENU_OPEN_FRAMES`) ; les menus s'ouvrent dans l'ordre où les jauges se sont remplies (`menuQueue_`).
- Approché : mesuré (9 à 19 images dans la vidéo), le mécanisme du jeu n'est pas porté.

### B05 — Mode d'attente (haute)
- [x] Jeu (`UpdateBattleTime`) : en mode Wait, le temps ne s'arrête que si le programme de combat l'a arrêté (`w7e2f41`) : A sur une commande qui ouvre une liste l'arrête, il repart au retour aux commandes, au ciblage de Fight et à la fermeture du menu. Les actions déjà en file continuent.
- Correctif : `BattleScreen::tickTime` (temps arrêté dans les listes et le ciblage d'une entrée de liste) ; le défaut reste Wait.

### B06 — Délai entre la validation et l'exécution (haute)
- [x] Jeu : une commande attend son délai (`CmdDelayTbl`, 16 pour Magitek : le compteur gagne la moitié de la constante ATB à chaque tic, ≈ 50 images), puis son tour dans la file des actions, exécutées une à une.
- Correctif : `queueCommand`, `waiting_`, `actions_`, `runActions`, `FF6_CMD_DELAY_TABLE` ; côté `Battle`, `setQueuedTurns` met en file les tours des monstres et de ceux qui agissent seuls (`runTurn`), `beginAction`/`endAction` gardent la jauge pleine jusqu'à la fin de l'action.

### B07 — File des menus (haute)
- [x] Jeu : le menu du personnage suivant s'ouvre dès la validation du précédent, pendant que les actions attendent ou se jouent.
- Correctif : les menus sont indépendants des messages et des animations (`update`).

### B08 — Découpage d'une action Magitek (haute)
- [x] Jeu (0 = début de l'encadré du nom) : nom 63 images, armure qui sort à + 121, rayon à + 166, boules de feu à + 199, retour de l'armure, chiffre à + 277, dissolution à + 284, jauge vidée à + 328.
- Correctif : le nom reste seul la durée de la vitesse des messages (`nameTimer_`), l'animation part `FF6_BATTLE_CAST_NAME_GAP` images après ; le personnage qu'une animation a emmené revient en `FF6_BATTLE_STEP_BACK_FRAMES` (`stepBack_`), les chiffres attendent encore `FF6_BATTLE_AFTER_STEP_FRAMES` ; la jauge se vide `FF6_BATTLE_ACTION_END_FRAMES` après. Moteur : nom + 0 à + 64, rayon + 166, boules de feu + 198, chiffre ≈ + 277, dissolution 6 images après.
- Approché : les écarts sont mesurés, pas portés du script graphique du jeu.

### B09 — Attaque d'un garde (haute)
- [x] Jeu : avant d'agir, le monstre clignote en noir à contour blanc, 4 images sur 8 deux fois (`FlashActiveMonster`, `LoadActiveMonsterFlashPal`) ; son coup part à + 63, le chiffre à + 95. Son attaque spéciale montre le nom de sa table (`MonsterSpecialName`, CF/D0D0 : « Critical » pour le garde) en grande police pendant 32 images.
- Correctif : `BattleGfx::monsterFlash`, `monsterFlash_`, `blowLead_` (`FF6_MONSTER_BLOW_LEAD`) ; noms des attaques spéciales ajoutés au pack (`battle.ff6data`, version 43), `largeName`.

### B10 — Sortie du combat (moyenne)
- [x] Jeu (`TerminateBattle`) : 9 images après le dernier message, luminosité −1 par image (15 images), écran éteint pendant que la carte se recharge (52 images mesurées), puis le fondu entrant du champ (15 images ; les premiers paliers paraissent noirs dans Ares).
- Correctif : `FF6_BATTLE_WIN_END_FRAMES`, `battleExitFrame_` (`FF6_BATTLE_EXIT_BLACK_FRAMES`).

## Fenêtres et menus

Les positions de cette section sont en pixels du jeu, mesurées sur la vidéo recalée (voir « Reproduire la comparaison ») ; celles de la première version de ce fichier venaient d'une extraction qui étirait l'image d'environ 7 % et sont fausses.

### B11 — Dégradé des fenêtres (moyenne)
- [x] Jeu : fond et cadre assombris ligne à ligne ; moteur (avant) : aplat (49,49,140).
- Cause : `LoadMenuGfx` éclaircit chaque canal des couleurs des fenêtres de 8 (`_c1417e`), puis la table HDMA de la mathématique des couleurs (`w7e8993`) retranche à BG2 un niveau toutes les 3 lignes à partir de la ligne 151 ; la fenêtre du haut est sur BG1, un niveau toutes les 2 lignes à partir de la ligne 8 (`InitMsgWindowHDMA`). Le texte (BG3) n'est pas touché.
- Correctif : `MenuScreen::windowPx` (fenêtre au pixel, avec le dégradé), constantes `FF6_BATTLE_WINDOW_BRIGHTEN`, `FF6_BATTLE_GRADIENT_*`, `FF6_BATTLE_MESSAGE_GRADIENT_*`.

### B12 — Position et hauteur des fenêtres du bas (moyenne)
- [x] Jeu : fenêtres de y=152 à 215 ; monstres x=8–103, personnages x=104–247, commandes x=16–95 (la fenêtre des monstres dépasse de chaque côté) ; lignes de texte à y=164 + 12k ; noms des personnages à x=112, PV (4 chiffres) à x=168.
- Moteur (avant) : fenêtres de 0 à 103 et de 104 à 255, PV à x=160.
- Correctif : `FF6_BATTLE_*_WINDOW_*`, `FF6_BATTLE_TEXT_*`, `FF6_BATTLE_HP_PX` ; `drawMonsters`, `drawParty`, `drawLists`.

### B13 — Fenêtre du haut (moyenne)
- [x] Jeu : lignes 8 à 39 ; nom d'une attaque dans une boîte centrée x=56–199, texte centré au pixel ; autres messages x=8–247, texte à x=16 ; texte à y=19.
- Correctif : `currentName_` (un message `Cast`), `FF6_BATTLE_NAME_BOX_*`, `FF6_BATTLE_MESSAGE_BOX_*`, `FF6_BATTLE_MESSAGE_TEXT_*`.

### B14 — Noms des personnages (moyenne)
- [x] Jeu : « ????? » (Terra n'a pas encore de nom), « WEDGE », « VICKS », en majuscules.
- Cause : `GameRules::actorName` ne gardait que les lettres (« ????? » devenait vide) et mettait les noms en minuscules ; `characterName` aussi.
- Correctif : les noms de la ROM tels quels (`FF6_MENU_CODE_QUESTION`), noms par défaut en capitales comme la version US.

### B15 — Couleur des noms (moyenne)
- [x] Jeu : blanc ; jaune seulement pour le personnage dont le menu est ouvert (`_c14780`) ; pas d'autre couleur.
- Correctif : `drawParty`.

### B16 — Jauges ATB (moyenne)
- [x] Jeu (`DrawGaugeText`) : bout gauche à x=200, quatre cases de 8 pixels (208–239), bout droit à 240 ; `(valeur >> 3) + 1` pixels de l'octet haut de la jauge ; palette 5 (gris, blanc) pendant le remplissage, palette 2 (bleu, jaune) quand elle est pleine, celles de `BattleFontPal` (ED/62C0).
- Moteur (avant) : cinq cases de quatre pas, palette des menus.
- Correctif : `drawParty` ; `BattleFontPal` est ajoutée au pack (`battlegfx.ff6data`, version 42) et `MenuScreen::setBg3Palette` la fait servir au texte des combats.
- Reste : la jauge pleine passe au jaune trop tôt, voir B04.

### B17 — Liste des monstres (moyenne)
- [x] Jeu : chaque nom une fois, dans l'ordre des places, quatre au plus (`UpdateMonsterGfxBuf`) ; nom à x=16.
- Correctif : `BattleScreen::monsterNames`.

### B18 — Commandes à emplacements fixes (moyenne)
- [x] Jeu : les quatre emplacements gardent leur place, une commande absente laisse un vide que le curseur saute ; texte à x=32, main à x=16.
- Correctif : `openCommands`, `updateCommand`.

### B19 — Liste Magi Tek (moyenne)
- [x] Jeu (`DrawMagitekListText`) : deux attaques par ligne, à places fixes ; Terra a les huit (`TerraMagitekAttackTbl`), les autres Fire Beam, Bolt Beam, Ice Beam, Heal Force (`DefaultMagitekAttackTbl`) ; sans coût ; fenêtre x=8–247 à partir de y=156, texte à x=32 et 136, lignes à 168 + 12k.
- Correctif : `openMagitek`, `columns_`, `FF6_MAGITEK_DEFAULT_LIST`, `FF6_BATTLE_LIST_*`.

### B20 — Ouverture des listes (basse)
- [x] Jeu : la fenêtre de la liste monte du bas en ≈ 8 images (t1756–1764) par-dessus les fenêtres, la main paraît ensuite (t1769).
- Correctif : `listOpening_` (`FF6_BATTLE_LIST_OPEN_FRAMES`), la fenêtre des commandes reste dessous pendant la montée.
- Approché : la montée est linéaire et le texte monte avec la fenêtre ; dans le jeu le texte (BG3) défile à part.

### B21 — Ciblage (moyenne)
- [x] Jeu : la liste reste affichée ; la main d'un monstre est à son bord droit moins 8, à mi-hauteur, tournée vers la gauche ; celle d'un personnage 16 pixels à sa gauche, 8 sous son haut (`UpdateTargetCursors`) ; le curseur part du monstre le plus à droite (`_c17922`), le garde du haut.
- Correctif : `drawHands`, `beginTarget`, `targetFromList_`, `MenuScreen::hand(x, y, flip)`.

### B22 — Pointeur ▼ (basse)
- [x] Jeu : flèche au-dessus du personnage dont le menu est ouvert (`DrawCursorSprites`), sprite de la tuile 0x2E de la planche des statuts dans les couleurs de la main, 24 lignes au-dessus de son centre, 12 de plus sur une armure (`_c134a5`).
- Correctif : `BattleGfx::statusSprite`, `FF6_BATTLE_ARROW_*`.

### B23 — Position de la pile d'armures (basse)
- [x] Jeu : le sprite d'un personnage commence 8 lignes au-dessus de sa position (`CharSpriteData`) ; le moteur le dessinait à sa position, 8 pixels trop bas. L'écart horizontal de la première version venait de l'extraction de la vidéo.
- Correctif : `FF6_CHAR_SPRITE_DY` (dessin du personnage, de son armure et de son ombre).

## Attaques et effets

### B24 — Fire Beam : étendue du rayon (haute)
- [x] Jeu : bande rouge de x=0 à x=115 seulement (jusqu'au canon), épaisseur 7 px à t2171 → 24 px à t2200 ; son halo (bg3, 256 × 48 px) couvre la même longueur, pas plus.
- Moteur (avant) : x=0 à 255 (traverse l'armure jusqu'au bord droit), épaisseur 8 px à f11664 → 24 px à f11691 ; le halo de bg3 reparaissait à droite du canon.
- La hauteur du rayon suit la cible (≈ 80 pour le garde du haut, ≈ 120 pour le garde du bas) : l'écart vertical vient de B21.
- Cause : `BattleScreen::drawAnimation` faisait faire le tour de l'écran à bg1 et bg3 toutes les 256 px. Dans le jeu, l'animation passe bg1 en tuiles de 16 × 16 (`ora #$10` de `w7e896f`, début de `FightCmdAnim` et des animations magiques) et la carte est de 32 × 32 tuiles : la couche fait le tour tous les 512 px. Une bande de 256 px centrée à x=−13 (−141 → 115) ne se voit donc qu'une fois, coupée au bord gauche.
- Correctif : `FF6_BATTLE_BG_PERIOD` (512) dans `drawAnimation` ([battle_screen.cpp](engine/src/battle/battle_screen.cpp)).
- Reste : la longueur visible dépend de la cible (le moteur vise le garde du bas, l'armure va plus loin : 87 px au lieu de 115), voir B21.

### B25 — Fire Beam : gant blanc-cyan (reclassé dans B07 et B21)
- [x] C'est la main de ciblage de Wedge pendant l'action de Vicks : réglé par B07 (menus pendant les actions) et B21 (main sur le sprite).

### B26 — Fire Beam : boules de feu (moyenne)
- [x] Jeu : trois boules successives, chacune de ≈ 28 px qui se contracte à ≈ 8 px en ≈ 8 images : t2204 (x=62–89), t2212 (x=54–80), t2221 (x=50–67).
- Moteur (avant) : une boule (x=34–59, 26 px à f11694, 8 px à f11700).
- Cause : la fonction d'initialisation 0x4E (Fire, Bolt, Ice Beam et Tek Laser) copie le fil du sprite deux fois avec 8 images d'écart (`CopyThread`) ; `kCopies` ([attack_anim_player.cpp](engine/src/battle/attack_anim_player.cpp)) ne la connaissait pas. Correctif : `{0x4E, 8, 2}`. Le script décale ensuite chaque fil de 1, 9 et 17 px vers l'avant.

### B27 — Flash orange (moyenne)
- [x] Jeu : aucun flash au début de Fire Beam.
- Moteur (avant) : le champ était teinté en orange à la validation (f11604 : 19 520 pixels rouges), pendant quelques images (`flashTimer_`).
- Correctif : le flash de l'élément n'existe plus pour un nom d'attaque (`Message::flash` n'est plus demandé par le cas `Cast` de `BattleScreen::collectEvents`). Seul le coup critique garde son flash blanc (animation 0x1A8 du jeu).

### B28 — Textes de combat inventés (haute)
- [x] Jeu : seul le nom d'une attaque nommée s'affiche. Pas de texte pour un coup de garde, pas de « loses HP », pas de « is defeated ».
- Moteur (avant) : « Guard hits Vicks for 6 », « Guard loses 127 HP », « Guard is defeated ».
- Correctif : `BattleScreen::collectEvents` met des messages sans texte pour les coups, les critiques, les dégâts, les soins, les PM et les chutes ; ils gardent leur effet, leur chiffre et leur durée. Un coup manqué donne le mot Miss (B30). Restent les textes que le jeu affiche aussi (noms d'attaques, statuts, dialogues).
- La pause entre le chiffre et la chute d'un monstre est de 6 images, puis 32 images de fondu (B31) ; les autres messages sans texte durent `FF6_BATTLE_MESSAGE_FRAMES` (40) : le déroulé complet est celui de B08 et B09.

### B29 — Effet du coup d'un garde (moyenne)
- [x] Jeu : trait blanc horizontal de 7 images (court, long, court) qui traverse la pile (x=190 → 256, y≈ 97) puis la cible encaisse.
- Moteur (avant) : étoile blanche (animation de coup n° 0x08).
- Cause : le coup d'un monstre prenait toujours la première entrée des attaques de monstres. Le jeu lit l'octet 26 de ses propriétés (« objet pour les graphismes »), ajoute 1 (`inc` de `_magicpunch`, 0xFF devient 0 : les poings) et prend cette entrée de `WeaponAnimProp` : pour le garde (octet 0), l'entrée 1, dont le coup est le script 0x3E, une ligne horizontale. Même règle pour un personnage (objet en main + 1) : le moteur prenait le numéro de l'objet tel quel, décalé d'un cran.
- Correctif : [battle_screen.cpp](engine/src/battle/battle_screen.cpp) (`collectEvents`, cas `Hit`), `FF6_ATTACK_WEAPON_FISTS` (0) et `FF6_ATTACK_WEAPON_ENTRIES` (0x5D) dans [constants.hpp](engine/src/constants.hpp) ; la fonction ne lance plus de clignotement du monstre touché quand le jeu a son animation.
- Reste : la ligne est ≈ 13 px plus bas que dans le jeu (7 px viennent de B23) et arrive 22 images avant le chiffre au lieu de 32 (B09).

### B30 — Chiffres de dégâts (moyenne)
- [x] Jeu : blancs (verts pour un soin), ils rebondissent (la table `DmgNumBounceTbl` : montée de 16 px en 12 images, chute, petit rebond de 8 px), visibles 38 images quand le monstre meurt (« 129 » de t2282 à t2319), 64 sinon.
- Moteur (avant) : jaunes, posés sur la grille de tuiles de 8 px, sans mouvement.
- Correctif : des sprites de 8 × 8 comme ceux du jeu (`DrawDmgNumSprites`, `BattleGfx::numeral`) : tuiles de `StatusGfx` (la décompression de D2/E000 va maintenant jusqu'à 0x1980 : le mot Miss à 0x1800, les chiffres à 0x1840), palettes de statut 12 (blanc) et 13 (vert), centrés sur le milieu du combattant, à 8 px au-dessus du bas d'un monstre (au pied d'un personnage), 4 au plus, retirés après 64 images ou quand le monstre n'est plus montré. Pack v41 (`PACK_VERSION` des outils et `FF6_PACK_VERSION` du moteur) : il faut regénérer `output/pack`.
- Reste : les positions dépendent de celles des combattants (B02, B23).

### B31 — Mort d'un monstre (moyenne)
- [x] Jeu : le sprite devient violet tramé et s'estompe en 32 images (t2288 → 2320 ; la teinte n'est nette que de t2289 à 2301), 6 images après le chiffre ; le chiffre disparaît avec lui.
- Moteur (avant) : le sprite s'éparpillait en pixels sans changer de teinte.
- Correctif : `MonsterDeathAnim` du jeu. Chaque couleur de la palette du monstre est remplacée par un violet (`MonsterDeathPal`, `FF6_MONSTER_DEATH_PALETTE`), qui perd un cran de rouge, de vert et de bleu à chaque image, et le sprite est ajouté au champ (`BattleGfx::monsterFading`, `MenuScreen::imageMath` dans `drawField`). `FF6_BATTLE_DEATH_FRAMES` (32) et `FF6_BATTLE_DEATH_DELAY` (6).

## Fin de combat

### B32 — Messages de victoire (haute)
- [x] Jeu (`WinBattle`) : « Got 32 Exp. point(s) » jusqu'à l'appui d'une touche (code 0x07 du texte), puis « VICKS gained a level », « WEDGE gained a level » (de la dernière place à la première), « Got 96 GP », chacun la durée de la vitesse des messages (`MsgSpeedTbl` : 64 images au réglage par défaut), dans la grande police des dialogues, fenêtre fermée ≈ 19 images entre deux ; le premier ≈ 143 images après la fin de la dernière action.
- Correctif : `BattleScreen::finish`, `victory_`, `Message::fixed` et `keyWait`, `MenuScreen::largeText`, `setMessageSpeed` ; la fenêtre « Victory! » n'existe plus.

## Affichage

### B33 — Pixels non carrés (basse)
- [x] Option : `--aspect 8:7` ou F9 affiche les pixels 8:7 comme Ares ; le défaut reste carré, les captures aussi (`SdlRenderer::setWideAspect`).

### B34 — Courbe de couleurs (basse)
- [x] Option : `--ares-colors 1` ou F8 applique une courbe gamma de 1,15 ajustée sur les couleurs de la vidéo (noir et blanc inchangés) ; désactivée par défaut, sans effet sur les captures (`SdlRenderer::setAresColors`).

### B35 — Danse de victoire (moyenne)
- [x] Jeu (`CharVictoryAnim`) : 77 images après la dernière action (t2740 → t2817), la fanfare et un tour de chaque personnage (4 images par pose, armure comprise), puis bras levés en boucle ; le moteur gardait la pose de combat car la pose de victoire attendait la fin des messages.

### B36 — Armure Magitek au retour (moyenne)
- [x] L'armure restait tournée vers les monstres quand le pilote revenait à sa place après une attaque : elle suit maintenant le sens de la pose du pilote.
- [x] Au retour qui suit une animation (`STEP_BACK`, `magic_back`), le personnage glissait sans se retourner. Comme dans la vidéo (Vicks après Fire Beam, t2228–2258), il se tourne vers sa place s'il regarde de l'autre côté, marche (`WALKING_FORWARD`, et l'armure marche : `wMagitekAnimType` 1), puis se retourne à l'arrivée.

### B37 — Animations coupées par un bouton (haute)
- [x] Un A pendant une animation (par exemple Fire Beam, en choisissant la commande suivante) l'arrêtait : l'armure ne bougeait pas, seule l'explosion et les dégâts restaient. Le jeu ne coupe ni les animations ni les messages ; seul un message qui attend une touche (l'expérience) y répond.

### B38 — « Got » sans nom d'objet (haute)
- [x] Les messages « Got … », « Stole … » écrivaient le nom de l'objet depuis son premier octet, celui de l'icône : pour un objet sans icône (0xFF, comme Tonic), le nom s'arrêtait aussitôt. Le jeu (`DlgTextCmd_0e`) saute l'icône et s'arrête au premier 0xFF (`BattleScreen::messageItemName`).

### B39 — Tenaille : attaque à reculons (moyenne)
- [x] En tenaille, un personnage qui attaquait le monstre dans son dos avançait à reculons. Le jeu (`magic_front`) le tourne d'abord vers sa cible ; il reste tourné de ce côté ensuite (`BattleScreen::faceTarget`).

### B40 — Portraits de Vicks et Wedge (moyenne)
- [x] Le menu montrait les portraits de Leo et Banon. Le jeu (`LoadPortraitGfx`) prend le portrait du sprite que porte le personnage (`PortraitGfxPtrs`) : le soldat pour Vicks et Wedge, Terra pour Terra en Esper, le fantôme, Celes en général ; Locke garde le sien, un Imp a celui de l'Imp (`wornPortrait`).

### B41 — PNJ animés du champ (moyenne)
- [x] Un PNJ dont les bits 5–7 du dernier octet ne sont pas nuls a une animation à lui (`InitNPCs`, `UpdateObjFrame`) : les bits 0–1 donnent alors le genre (une image, miroir, 2 ou 4 images), les bits 5–7 la première image (`ObjSpecialTileTbl` : `SPECIAL_ANIM_1`, `DEAD_HORZ`), le haut de l'octet du mouvement la vitesse. Le moteur le dessinait comme un personnage tourné (morceaux de flamme dans la mine de Narshe, où c'est l'étoile d'un point de sauvegarde) ; il suit maintenant le jeu (`animatedFrame`).

### B42 — Points de sauvegarde infranchissables (haute)
- [x] Le PNJ de l'étoile a la priorité de sprite haute : le jeu ne le met pas dans la carte des objets (`InitNPCMap`, ni ceux qui défilent avec BG2), le groupe marche dessus et le déclencheur de la case rend la sauvegarde possible. Le moteur le prenait pour un obstacle. `Npc::solid` suit le jeu ; ces sprites passent devant les autres (priorité basse : derrière).

### B43 — Temps de jeu perdu au chargement (basse)
- [x] Le temps du menu venait du compteur d'images de la partie, qui repartait de 0 à chaque chargement. La sauvegarde garde maintenant `playFrames` et le chargement le reprend (une ancienne sauvegarde repart de 0:00).

### B44 — Whelk : tête qui ne se cache pas, boss immortel (haute)
- [x] `F5` (entrée / sortie des monstres) : le moteur marquait le monstre « mourant » sans animation, ce qui le laissait affiché pour toujours. L'événement porte maintenant le type d'animation (`MONSTER_ENTRY_EXIT_ANIM`) ; l'écran joue le script du jeu (`MonsterEntryScriptPtrs` / `MonsterExitScriptPtrs`, constantes `FF6_MONSTER_ENTRY_ANIMS` et `FF6_MONSTER_EXIT_ANIMS`, avec son son), garde le monstre qui sort jusqu'à la fin de son animation et cache celui qui entre jusqu'à son début.
- [x] Un monstre que le coup tue exécute encore son script de riposte, une fois (`ExecAIRetal`, `w7e3a56`) : `if_self_dead` puis `boss_death` cachent tous les monstres et finissent le combat. Le moteur sautait la riposte d'un monstre mort : la coquille faisait revenir la tête à 1 PV, sans fin.
- [x] `$80/$80` (`mod_boss_death_pal`) : sans le bit 0x10 le décalage soustrait aux couleurs grandit (le boss vire au rouge puis au noir) ; le moteur ne prenait que l'autre cas.
- [x] Le boss devenait noir au lieu de disparaître : `BOSS_DEATH_BG1` met les sprites en priorité 0 (derrière le décor) et additionne l'écran secondaire (les sprites) au décor (`color_math {ADD, SUBSCREEN}, BG2, SPRITE`) ; les monstres s'ajoutent donc au décor et s'y fondent quand leurs couleurs tombent au noir (`AttackAnimPlayer::fieldBlend`, `ghostMonsters`).
- [x] Rideau de `FADE_DOWN_EXIT` / `FADE_UP_ENTRY` : la bande de BG3 s'ajoute à l'écran secondaire (BG2 et sprites). Les monstres sont des sprites ; l'init `$1d` (`MonstersToBG1`, aussi par `$7b`, `$7c` et d'autres) copie le monstre visé sur BG1, qui sort donc de l'écran secondaire : la bande l'efface de haut en bas (ou le fait apparaître de bas en haut), les autres restent (`AttackAnimPlayer::bg1Monsters`, tampons `charPixels_` et `monsterPixels_`).

### B45 — Vicks et Wedge couchés après une animation (moyenne)
- [x] `attacker_frame NONE` (`$81`/`$82` avec 0) rend le personnage à son action (`UpdateCharFrame` ne prend une image que si elle n'est pas nulle) ; le moteur affichait l'image 0 (`DEAD_HORZ`) et la gardait après l'animation : la tête des soldats en armure à l'envers.

### B46 — Dialogues de combat trop rapides (moyenne)
- [x] Les dialogues des monstres (`F3`, `MONSTER_DLG`) s'écrivent une lettre par image puis restent la durée de la vitesse des messages (`MsgSpeedTbl`) ; les longs dialogues des scènes (`BATTLE_DLG`) montrent une page de lignes, lettre par lettre, et attendent le bouton au code 7 une fois écrits. Le moteur montrait chaque ligne d'un coup, 40 images.
- [x] Une scène (événement de combat) suspend la boucle du combat : le temps s'arrête, les menus ne répondent plus et le bouton va au dialogue (`BattleScreen::sceneHolds`). Le moteur laissait ouvrir un menu (Magi Tek au début du combat contre Whelk) qui prenait le bouton : le dialogue restait bloqué.

### B47 — Danse de victoire après une scène (moyenne)
- [x] `end_battle` (`FB 02`, fin immédiate) ne passe pas par `WinBattle` : ni fanfare, ni danse, ni messages (scène de Tritoch, Leap de Gau). Un combat sans fanfare n'a pas de danse non plus (`BTL_GFX::VICTORY`).

Remarque : un Tonic ne ranime pas un personnage à 0 PV, comme dans le jeu (`CheckCanUseItem` : un personnage KO n'accepte que la Fenix Down).

## Conforme

- Fond de combat, PV de départ (63, 68, 70), vitesse de remplissage de l'ATB, valeurs d'expérience (32) et d'argent (96), ordre de grandeur des dégâts (6 à 8 pour les gardes, 127 à 129 pour Fire Beam).
- Avant le combat : le trajet et les dialogues sont à ± 5 images (ouverture du dialogue, marche, déclenchement des gardes).

## Hors périmètre (constaté en passant)

Fenêtres de dialogue du champ : même écart de style (dégradé), largeur x=0–254 contre x=8–247, texte à x=12 contre 20, ouverture animée d'environ 8 images (une barre qui grandit) et fermeture d'environ 12 images (le texte disparaît puis la fenêtre se referme) contre un affichage et une fermeture immédiats.

## Non comparable

- La piste audio de la vidéo est vide : sons et musiques ne peuvent pas être comparés.
- Hasard : voir « Décisions ».

## Reproduire la comparaison

- **Vidéo** : Ares masque le débordement : la zone de 1304 × 998 pixels de la fenêtre montre les colonnes 8 à 247 et les lignes 4 à 214 du jeu. `ffmpeg` avec `crop=1304:998:160:150,scale=240:211:flags=neighbor,pad=256:224:8:4` donne des images aux coordonnées du jeu (calées sur le fond, la fenêtre du haut et celles du bas) ; `t` = arrondi de pts × 60. La première extraction (`scale=256:224`) étirait l'image d'environ 7 % : les positions mesurées avec elle sont fausses, les durées restent bonnes.
- **Moteur** : programme sans fenêtre qui lie `libff6_game.a`, `libff6_core.a` et `libff6_audio.a` avec une nouvelle partie et les touches ci-dessous (image → touche). Ces outils sont dans .scratch/, hors dépôt.

| f | Touche |
|---|---|
| 900, 1000 | A (titre, New Game) |
| 10300 | A (passe l'ouverture) |
| 10694 | A (ferme le premier dialogue, 86 images après son ouverture) |
| 10823–11010 | Haut maintenu (la marche vers le nord après le pas scripté de f10799) |
| 11132 | A (ferme le dialogue des gardes) |

- Le combat s'affiche pleinement à f11334 (fondu de f11297 à f11333 ; f11190 avant B01). Pour le mode Active, mettre à zéro le bit `FF6_CONFIG_WAIT` de `config_.battle`.
