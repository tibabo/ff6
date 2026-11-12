# Bugs : premier combat de Narshe

Écarts entre l'enregistrement du jeu sur Ares ([narshee-first-battle.mov](narshee-first-battle.mov)) et le moteur, sur le combat des deux gardes de Narshe (Terra, Wedge et Vicks en armures Magitek). Ce fichier est la liste des écarts ; une case cochée est corrigée (section « Attaques et effets » : B24 et B26 à B31).

Conventions :

- **Jeu** = l'enregistrement ; **moteur** = le moteur natif.
- **image** = 1/60 s. **t** = numéro d'image de la vidéo depuis son début (arrondi de pts × 60) ; **f** = numéro d'image du moteur depuis le lancement d'une nouvelle partie, une image par `Game::tick`.
- Les durées sont mesurées image par image sur la vidéo ramenée à 256×224 ; ± 1 image.
- Une case à cocher par bug ; priorité : haute (déroulé), moyenne (menus), basse (cosmétique).

## Décisions à prendre avant de corriger

- [ ] **Mode d'attente** : l'enregistrement est-il en Active, ou le Wait du jeu ne fige-t-il que les sous-menus ? Voir B05.
- [ ] **Rapport 8:7** : le moteur affiche en pixels carrés, Ares en 8:7. Voir B33.
- [ ] **Courbe de couleurs** : demi-teintes plus sombres dans la vidéo (émulation de couleur d'Ares ?). Voir B34.
- [ ] **Hasard** : jauges initiales, dégâts et cibles des gardes dépendent de la table de hasard du jeu (`RNGTbl`, C0/FD00) ; sans elle, 129 contre 127, 6/7 contre 6/8 et l'ordre des personnages prêts ne peuvent pas être identiques.

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
- [ ] Jeu : mosaïque en dents de scie pendant 31 images (rampes de 0 à ≈ 14 px qui retombent à zéro à t1283 et t1291, la dernière monte jusqu'à ≈ 36 px), coupure nette au noir, noir de 104 images, fondu entrant de 36 images. Total 171 images.
- Moteur : fondu sortant ≈ 15 images (f11161–f11175), fondu entrant ≈ 15 (f11176–f11190). ≈ 30 images, ni mosaïque ni noir.
- Piste : `Game::launchBattle` ([game.cpp](engine/src/game.cpp#L1676)), `Game::startFade` ([game.cpp](engine/src/game.cpp#L533)) ; la mosaïque du champ existe pour les événements (`Game::applyStage`, [game.cpp](engine/src/game.cpp#L562)).

### B02 — Entrée des gardes (haute)
- [ ] Jeu : aucun monstre pendant le fondu ; le garde du bas apparaît à x=0–8 à t1469 et les deux sont en place à t1476 (22 images après la fin du fondu, 7 images d'entrée).
- Moteur : présents dès la première image du combat.
- Piste : le moteur sait déjà faire entrer un monstre (`BattleEvent::Kind::Entry` dans [battle.hpp](engine/src/battle/battle.hpp)) ; à brancher sur ce combat.

### B03 — Départ et valeurs initiales de l'ATB (basse)
- [ ] Jeu : jauges initiales 19, 25, 26 sur 34 px (56, 74, 76 %), immobiles jusqu'à t1453, elles repartent à t1454 (+ 7 après le fondu) à ≈ 0,085 px/image (34 px en ≈ 400 images).
- Moteur : 79, 83, 63 %, départ à f11192 (+ 2 après le fondu), vitesse équivalente (Terra ≈ 412 images pour remplir, Vicks ≈ 390).
- Les valeurs initiales sont tirées au hasard : voir « Décisions ».

### B04 — Délai entre jauge pleine et ouverture du menu (moyenne)
- [ ] Jeu : jauge pleine (blanche), puis jaune 10 à 19 images plus tard, puis menu. Vicks : pleine ≈ t1542, jaune t1557, menu t1565 (+ 23). Wedge : pleine ≈ 1557, jaune 1576. Terra : pleine 1623, jaune 1632.
- Moteur : le menu s'ouvre à l'image où la jauge atteint 100 % (Wedge à f11260).

### B05 — Mode d'attente (haute, à trancher)
- [ ] Jeu : le menu principal ne fige pas le temps. Un garde frappe Vicks à t1740, 175 images après l'ouverture de son menu (t1565) ; le garde du haut commence à attaquer à t1866 pendant que le curseur de cible de Vicks est affiché.
- Moteur : le défaut est Wait ([constants.hpp](engine/src/constants.hpp#L721)) et `BattleScreen::keepsMenu()` ([battle_screen.cpp](engine/src/battle/battle_screen.cpp#L1355)) n'avance aucune jauge tant qu'un menu (commandes, liste, cible) est ouvert : aucune attaque de garde en 1800 images avec un menu laissé ouvert. En mode Active, les gardes attaquent à f11410 (+ 220 après le fondu) puis f11584.
- Piste : `UpdateBattleTime` du jeu (`src/battle/atb.asm`) saute la mise à jour quand « temps arrêté par le programme de combat » ET mode Wait ; reste à voir qui arrête le temps dans `src/btlgfx/`.

### B06 — Délai entre la validation et l'exécution (haute)
- [ ] Jeu : 122 images entre la validation de la cible et l'encadré du nom pour Vicks (t1883 → 2005) ; 132 pour Wedge (t2261 → 2393, l'action de Vicks se termine à t2333).
- Moteur : 1 image.
- Piste : `CalcCmdDelay` et `CmdDelayTbl` (`src/battle/action_1.asm`) : Magitek = 16 ; le compteur avance de moitié moins vite que la jauge (`src/battle/atb.asm`), soit ≈ 50 images. Reste ≈ 70 images à expliquer (file d'actions, fin du ciblage, séquence de début d'animation).

### B07 — File des menus (haute)
- [ ] Jeu : le menu du personnage suivant s'ouvre dès la validation du précédent (Wedge à t1883, Terra à t2261), pendant que les actions précédentes attendent ou s'exécutent.
- Moteur : le menu suivant s'ouvre après la fin de l'action et de ses messages (Terra à f11750 après l'action de Wedge, f11560–f11749).
- Piste : `BattleScreen::updateFighting` ([battle_screen.cpp](engine/src/battle/battle_screen.cpp#L1336)) ne consulte `readyCharacter()` que si `messages_` est vide.

### B08 — Découpage d'une action Magitek (haute)

| Étape | Jeu (Vicks, 0 = validation, t1883) | Moteur (Wedge, 0 = validation, f11601) |
|---|---|---|
| Nom de l'attaque | + 122 à + 185 (63 images) | + 1 à + 189 (188 images) |
| Pause | 58 images sans mouvement | aucune |
| L'armure sort de la pile | + 243 | pendant l'affichage du nom |
| Rayon | + 288 à + 318 | + 64 à + 91 |
| Boules de feu | + 321 à + 342 (3) | + 93 à + 100 (1) |
| Retour de l'armure | + 347 à + 379 | non mesuré |
| Chiffre des dégâts | + 399 (38 images) | ≈ + 109 à + 148 (dans le texte « Guard loses 127 HP ») |
| Dissolution du monstre | + 406 à + 418 | ≈ + 155 à + 180 |
| Jauge remise à zéro | + 450 | fin de l'action ≈ + 190 |

- [ ] Durée totale : 450 images (Vicks) et 479 (Wedge) contre ≈ 190 dans le moteur.

### B09 — Attaque d'un garde (haute)
- [ ] Jeu : le garde clignote en silhouette noire à contour blanc (2 × 4 images, t1642 et t1650), trait blanc horizontal de 5 images sur la pile à t1705–1709 (+ 63), chiffre à t1737 (+ 95), PV mis à jour à t1740 (+ 98) ; 2e garde : clignotement t1866 et t1874, PV à t1961 (+ 95).
- Moteur : coup sans texte (B28) de 72 images (f11411–f11483) : trait horizontal (B29) à f11420–f11429 (+ 10), chiffre à f11443 (+ 33), pas de clignotement du garde qui attaque ; PV déjà changés à f11410 dans l'état interne. Restent les délais (+ 63 et + 95 dans le jeu) et le clignotement.
- Voir aussi B28, B29, B30.

### B10 — Sortie du combat (moyenne)
- [ ] Jeu : après « Got 96 GP » (t3319), 9 images, fondu sortant de 15 images, noir de 52 images, fondu entrant du champ de 12 images ≈ 80 images.
- Moteur : fondu sortant ≈ 16 images (f11820–f11836), fondu entrant ≈ 14 (f11838–f11852), sans noir : ≈ 32 images.
- Piste : `Game::endBattle` ([game.cpp](engine/src/game.cpp#L1762)).

## Fenêtres et menus

### B11 — Dégradé des fenêtres (moyenne)
- [ ] Jeu : fond en dégradé vertical, (93,94,189) en haut → (0,0,35) en bas ; bordure : blanc pur (254,252,253) puis (188,188,188) puis (95,102,101) ; ombre en bas (77,77,77) puis (27,27,27).
- Moteur : aplat (49,49,140) ; bordure (206,231,231) puis (132,132,132) puis (41,49,49).
- Le dégradé du jeu vient d'une table HDMA de couleur ; voir la note « gradient window fill (HDMA) » du menu.

### B12 — Position et hauteur des fenêtres du bas (moyenne)
- [ ] Jeu : bord haut à y=157, jusqu'à y=223 (67 lignes), plus deux languettes (x=0–8 et x=93–101). Fenêtre de commandes x=9–92, fenêtre des personnages x=102–255.
- Moteur : y=152 à 215 (64 lignes) puis 8 lignes noires jusqu'à 223 ; commandes x=8–95, personnages x=104–255.
- Piste : `FF6_BATTLE_MONSTERS_Y` 19 et `FF6_BATTLE_WINDOW_ROWS` 6 ([constants.hpp](engine/src/constants.hpp#L1110)).

### B13 — Fenêtre du haut (moyenne)
- [ ] Jeu : deux formes. Nom d'une attaque : encadré centré x=51–204, y=4–37, texte centré. Messages de fin : pleine largeur x=0–255, y=4–38, texte à gauche.
- Moteur : un seul encadré x=8–247, y=8–39, texte à gauche (`FF6_BATTLE_MESSAGE_*`, [constants.hpp](engine/src/constants.hpp#L1107)).

### B14 — Noms des personnages (moyenne)
- [ ] Jeu : « ????? » (Terra n'a pas encore de nom), « WEDGE », « VICKS », en majuscules.
- Moteur : « Terra », « Wedge », « Vicks ».
- Piste : `text(characterName(c.id))` dans `BattleScreen::drawParty` ([battle_screen.cpp](engine/src/battle/battle_screen.cpp#L2388)).

### B15 — Couleur des noms (moyenne)
- [ ] Jeu : blanc ; jaune seulement pour le personnage dont le menu est ouvert ; pas de couleur pour le statut Magitek.
- Moteur : jaune pour tous les prêts, cyan pour un statut (Magitek compris).
- Piste : le test de couleur dans `drawParty` ([battle_screen.cpp](engine/src/battle/battle_screen.cpp#L2388)).

### B16 — Jauges ATB (moyenne)
- [ ] Jeu : remplissage de 34 px (x=213–245), lignes centrales y=174, 187, 200 (pas de 13), jaune quand la jauge est pleine ; remplissage fin ou épais selon le cas (par exemple Vicks au départ et après son action), à examiner.
- Moteur : ≈ 40 px (x=200–241), lignes y=168, 180, 192 (pas de 12), blanc quand pleine.
- Piste : `FF6_BATTLE_GAUGE_*` et la boucle de jauge de `drawParty`.

### B17 — Liste des monstres (moyenne)
- [ ] Jeu : « Guard » une seule fois (les noms identiques sont regroupés).
- Moteur : une ligne par monstre, « Guard » deux fois (`BattleScreen::drawMonsters`, [battle_screen.cpp](engine/src/battle/battle_screen.cpp#L2365)).

### B18 — Commandes à emplacements fixes (moyenne)
- [ ] Jeu : quatre emplacements. Terra : MagiTek / vide / Magic / Item. Wedge et Vicks : MagiTek / vide / vide / Item (pas de ≈ 12,7 px, Item à 3 lignes sous MagiTek).
- Moteur : liste compacte, les commandes absentes sont supprimées (`BattleScreen::openCommands`, [battle_screen.cpp](engine/src/battle/battle_screen.cpp#L1367)).

### B19 — Liste Magi Tek (moyenne)
- [ ] Jeu : fenêtre pleine largeur sur deux colonnes à places fixes : Fire Beam | Bolt Beam / Ice Beam / Heal Force (les attaques inconnues laissent un vide), sans coût.
- Moteur : colonne étroite avec les 8 attaques et un coût « 0 », Bio Blast à la place de Heal Force (`BattleScreen::openMagitek`, [battle_screen.cpp](engine/src/battle/battle_screen.cpp#L1791) ; largeur `FF6_BATTLE_LIST_W` 14).

### B20 — Ouverture des listes (basse)
- [ ] Jeu : le texte défile sur ≈ 8 images (t1759–1766) pendant que la fenêtre pleine largeur monte sur celle des personnages.
- Moteur : changement immédiat.

### B21 — Ciblage (moyenne)
- [ ] Jeu : la main est posée sur le sprite du monstre (t1800 : à côté du garde du haut) et la liste reste affichée ; le curseur démarre sur le garde du haut.
- Moteur : la main est dans la fenêtre des noms (`drawHands`, [battle_screen.cpp](engine/src/battle/battle_screen.cpp#L2452)) et le curseur démarre sur le garde du bas (premier monstre de la formation).

### B22 — Pointeur ▼ (basse)
- [ ] Jeu : un petit ▼ blanc au-dessus de l'armure du personnage dont le menu est ouvert (par exemple (225, 89) pour Vicks).
- Moteur : non vu.

### B23 — Position de la pile d'armures (basse)
- [ ] Jeu : cheveux de Terra x=203–214, y=58–67 ; pile x=199–236, y=61–134.
- Moteur : cheveux x=197–208, y=65–72 ; pile x=193–230, y=68–138 : 6 px trop à gauche, 7 px trop bas en haut de pile et 4 px en bas. Les gardes sont à ± 5 px.

## Attaques et effets

### B24 — Fire Beam : étendue du rayon (haute)
- [x] Jeu : bande rouge de x=0 à x=115 seulement (jusqu'au canon), épaisseur 7 px à t2171 → 24 px à t2200 ; son halo (bg3, 256 × 48 px) couvre la même longueur, pas plus.
- Moteur (avant) : x=0 à 255 (traverse l'armure jusqu'au bord droit), épaisseur 8 px à f11664 → 24 px à f11691 ; le halo de bg3 reparaissait à droite du canon.
- La hauteur du rayon suit la cible (≈ 80 pour le garde du haut, ≈ 120 pour le garde du bas) : l'écart vertical vient de B21.
- Cause : `BattleScreen::drawAnimation` faisait faire le tour de l'écran à bg1 et bg3 toutes les 256 px. Dans le jeu, l'animation passe bg1 en tuiles de 16 × 16 (`ora #$10` de `w7e896f`, début de `FightCmdAnim` et des animations magiques) et la carte est de 32 × 32 tuiles : la couche fait le tour tous les 512 px. Une bande de 256 px centrée à x=−13 (−141 → 115) ne se voit donc qu'une fois, coupée au bord gauche.
- Correctif : `FF6_BATTLE_BG_PERIOD` (512) dans `drawAnimation` ([battle_screen.cpp](engine/src/battle/battle_screen.cpp)).
- Reste : la longueur visible dépend de la cible (le moteur vise le garde du bas, l'armure va plus loin : 87 px au lieu de 115), voir B21.

### B25 — Fire Beam : gant blanc-cyan (reclassé dans B07 et B21)
- [ ] Jeu : petite forme blanc-cyan en forme de main à x≈ 88–103, juste devant le canon, de t2169 à t2220.
- Ce n'est pas un effet de l'attaque : c'est la main de ciblage de Wedge. Sa liste Magi Tek est ouverte (t2140), la main est sur « Fire Beam » (t2160), puis passe sur le garde du haut quand il choisit la cible (t2169) pendant que l'animation de Vicks se joue ; elle disparaît à la validation (t2221). Elle n'existe pas pendant l'action de Wedge (aucun menu ouvert). Rien à ajouter aux animations : voir B07 (file des menus) et B21 (main sur le sprite).
- Moteur : absente, faute de ces deux comportements.

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
- [ ] Jeu : quatre encadrés pleine largeur l'un après l'autre : « Got 32 Exp. point(s) » (203 images), « VICKS gained a level » (70), « WEDGE gained a level » (41), « Got 96 GP » (65) ; environ 190 images entre le chiffre du dernier garde et le premier encadré.
- Moteur : fenêtre « Victory! Exp: 32 Gil: 96 » qui attend A (`BattleScreen::drawResult`, [battle_screen.cpp](engine/src/battle/battle_screen.cpp#L2465)) et message « Vicks is now level 2 ». Les valeurs 32 et 96 sont les mêmes.

## Affichage

### B33 — Pixels non carrés (basse, à trancher)
- [ ] Jeu : Ares affiche en 8:7 (zone de jeu 1304 × 998, rapport 1,31).
- Moteur : pixels carrés (rapport 1,14) à cause de `SDL_RenderSetLogicalSize(256, 224)` ([sdl_renderer.cpp](engine/src/sdl/sdl_renderer.cpp#L119)).

### B34 — Courbe de couleurs (basse, à trancher)
- [ ] Jeu : demi-teintes plus sombres de 10 à 20 %, noir et blanc identiques : fond (33,33,54), (40,48,72), (54,70,101) contre (41,41,66), (49,57,82), (66,82,115) ; cheveux de Terra (118,162,119) contre (132,173,132). C'est probablement l'émulation de couleur d'Ares, pas une donnée du jeu.
- Moteur : `(c << 3) | (c >> 2)` brut.

## Conforme

- Fond de combat, PV de départ (63, 68, 70), vitesse de remplissage de l'ATB, valeurs d'expérience (32) et d'argent (96), ordre de grandeur des dégâts (6 à 8 pour les gardes, 127 à 129 pour Fire Beam).
- Avant le combat : le trajet et les dialogues sont à ± 5 images (ouverture du dialogue, marche, déclenchement des gardes).

## Hors périmètre (constaté en passant)

Fenêtres de dialogue du champ : même écart de style (dégradé), largeur x=0–254 contre x=8–247, texte à x=12 contre 20, ouverture animée d'environ 8 images (une barre qui grandit) et fermeture d'environ 12 images (le texte disparaît puis la fenêtre se referme) contre un affichage et une fermeture immédiats.

## Non comparable

- La piste audio de la vidéo est vide : sons et musiques ne peuvent pas être comparés.
- Hasard : voir « Décisions ».

## Reproduire la comparaison

- **Vidéo** : `ffmpeg` avec `crop=1304:998:160:150`, puis `scale=256:224:flags=neighbor` ; `t` = arrondi de pts × 60.
- **Moteur** : programme sans fenêtre qui lie `libff6_game.a`, `libff6_core.a` et `libff6_audio.a` avec une nouvelle partie et les touches ci-dessous (image → touche). Ces outils sont dans .scratch/, hors dépôt.

| f | Touche |
|---|---|
| 900, 1000 | A (titre, New Game) |
| 10300 | A (passe l'ouverture) |
| 10694 | A (ferme le premier dialogue, 86 images après son ouverture) |
| 10823–11010 | Haut maintenu (la marche vers le nord après le pas scripté de f10799) |
| 11132 | A (ferme le dialogue des gardes) |

- Le combat s'affiche pleinement à f11190. Pour le mode Active, mettre à zéro le bit `FF6_CONFIG_WAIT` de `config_.battle`.
