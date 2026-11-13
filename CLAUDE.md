# CLAUDE.md

Moteur natif C++17 de Final Fantasy VI (`engine/`) et outils Node qui génèrent ses données (`tools/`). Chantier en cours : reproduire fidèlement le premier combat de Narshe enregistré dans `narshee-first-battle.mov`. Les écarts sont listés dans `bugs.md` (B01–B34) : tous sont cochés ; les approximations (durées mesurées et non portées, index de départ du hasard) y sont notées.

Skills du projet (`.claude/skills/`) : `ff6-comparer-video` (mesurer un écart avec l'enregistrement), `ff6-porter-desassemblage` (trouver la règle du jeu dans everything8215/ff6 et la ROM), `ff6-boucle-combat` (déroulé et minutage d'un combat dans le moteur).

## Règles

- Répondre en français.
- Ne jamais committer sans demande explicite.
- Fichiers temporaires, journaux, clones de référence : dans `.scratch/` (ignoré par git), jamais dans `/tmp`.
- Constantes dans `engine/src/constants.hpp` (préfixe `FF6_`), pas de nombres magiques ; commentaires du code courts, en anglais.
- Quand un comportement change : mettre à jour les tests (`engine/tests`, `tools/*.test.mjs`), `README.md` (français sans accents), `docs/rendu-et-animations.md` (rendu des combats) et cocher `bugs.md`.

## Construire et tester

```sh
cmake -S engine -B build/engine -DCMAKE_BUILD_TYPE=Release   # une fois
cmake --build build/engine [--target ff6]                    # tout (ff6_core, ff6_game, ff6_audio, ff6_tests, ff6) ou une seule cible
FF6_PACK=output/pack ./build/engine/ff6_tests
FF6_ROM="$PWD/Final Fantasy 6 [US].smc" node --test $(find tools -name "*.test.mjs")
node ff6.mjs build-pack --rom "Final Fantasy 6 [US].smc" --out output/pack
```

- `build-pack` refuse d'écraser : déplacer l'ancien `output/pack` dans `.scratch/` d'abord.
- Quand les données du pack changent, augmenter ensemble `FF6_PACK_VERSION` (`engine/src/constants.hpp`) et `PACK_VERSION` (`tools/pack.mjs`) puis régénérer le pack ; un binaire ou un pack d'une autre version refuse de charger (« unsupported … file version »).
- La ROM, `output/` et `build/` ne sont pas versionnés.

## Outils de `.scratch/` (local, non versionné)

Prérequis : `python3` avec `numpy` et `Pillow`, `clang++`, `ffmpeg` (seulement pour extraire la vidéo), le moteur construit et un `output/pack` de la version du moteur. Les scripts ont le chemin `/Users/jpinkasfeld/workspace-gpfw/ff6/.scratch/` écrit en dur. L'en-tête de chaque script donne son usage exact ; les `*.out` et `*.png` sont des résultats d'analyses passées.

### Vidéo (`.scratch/vid/`)

- `game.rgb` : les 3211 images de la vidéo, brutes en RGB 256×224 **aux coordonnées du jeu** : Ares masque le débordement, la zone `crop=1304:998:160:150` montre les colonnes 8 à 247 et les lignes 4 à 214 ; elle est réduite au plus proche voisin en 240×211 et posée à (8, 4) (les bords sont noirs). Une ancienne extraction en 256×224 étirait l'image de 7 % : ses positions ne valent rien. `times.txt` : colonnes `n pts pts_time`. **tick = round(pts_time × 60)** : les `t…` de `bugs.md` sont des ticks, et l'indice d'image de la vidéo n'est pas le tick (des images ont été laissées tomber). Régénérer : `bash .scratch/vid/extract.sh`.
- **`mix.py`**, l'outil principal : planche mixte vidéo / moteur, à ouvrir ensuite comme une image.
  `python3 .scratch/vid/mix.py SORTIE.png COLONNES ECHELLE spec...` avec `spec` = `v<indice>` | `T<tick>` | `e<fichier.rgb>@<indice>`, suivi au choix de `:x,y,w,h` (rognage).
  Exemple : `python3 .scratch/vid/mix.py .scratch/cmp.png 2 3 T2195:0,60,256,80 e.scratch/drv/essai.rgb@242:0,60,256,80`
- Autres planches : `sheet.py` (vidéo ; temps `12.5s` ou indices), `esheet.py` (moteur : `NOM.rgb SORTIE.png COLS ECHELLE PREMIERE PAS indice...`).
- Détecteurs dans le temps, sur la vidéo (indices d'images ; ils affichent le tick des changements) : `reg.py video FICHIER a b x0 y0 x1 y1 SEUIL` (la zone change), `number.py` / `numpos.py` (chiffres blancs, dissolution magenta), `red.py` / `redrows.py` (rayon rouge), `bright.py` / `handcount.py` (pixels très clairs : main du curseur), `blink.py` / `flash.py` (clignotements), `gaugefill.py` (jauges ATB), `entry.py`, `guards.py`, `hair.py`, `win.py`, `txt.py`, `topbox.py`, `scroll.py`, `stats.py` / `analyze.py` (luminosité et écart entre images).
  Sur le moteur : `reg.py moteur FICHIER ...` (FICHIER = `.scratch/vid/game.rgb` ou `.scratch/drv/NOM.rgb`), `enumpos.py`, `emag.py`, `tim.py`, `logsum.py`.
- Mesures : `prof.py SPEC row|col N [a b]` (suites de couleurs le long d'une ligne ou d'une colonne), `boxes.py SPEC x0 y0 x1 y1 [seuil]` (boîtes des taches claires : texte, main), `ncc.py SPEC e<fichier>@<i> x0 y0 x1 y1 [r]` (décalage par corrélation des contours) ; SPEC = `T<tick>` | `v<indice>` | `e<fichier.rgb>@<indice>`.
- Pixels : `pix.py` (indice vidéo, valeurs hexa d'une zone), `row.py`, `col.py`, `colors.py`, `gauge.py` (`v<i>` ou `e<fichier>@<i>`). Écarts : `shift.py` / `shift2.py` (décalage dx,dy qui rapproche vidéo et moteur), `cmp.py` (écart moyen d'une zone).

### Pilote du moteur (`.scratch/drv/`)

`bash .scratch/drv/run.sh NOM PREMIERE DERNIERE [1] [a-b:n]`

- Rejoue, de l'image 0 à DERNIERE et sans fenêtre, le script de touches `.scratch/drv/NOM.txt` : lignes `image TOUCHES` (A confirmer, B annuler, M menu, S start, U R D L directions), `a-b U` pour maintenir une direction, `#` commentaire.
- Compile d'abord `dump.cpp` contre `build/engine/lib*.a` (`build.sh`, erreurs dans `build.out`) : `cmake --build build/engine` doit être passé.
- Écrit `NOM.rgb` (images PREMIERE..DERNIERE, 256×224×3 brut, indice = image − PREMIERE), `NOM.log` (état du combat à chaque image), `NOM.sum` (résumé du journal par `logsum.py` : modes, messages, PV, personnages prêts) et `NOM.snd`.
- `1` en 4e argument = mode Active (le défaut du moteur est Wait, qui fige l'ATB dans les menus ; la vidéo se joue en Active) ; `a-b:n` = appuyer sur A toutes les n images de a à b.
- Variables d'environnement : `FF6_ACTIVE`, `FF6_AUTO` (les deux arguments ci-dessus), **`FF6_AUTO_MENU=1`** (n'appuie que dans les menus du combat ; sans elle, un A pendant une animation ou un message la coupe), `FF6_LOG` (chemin du journal), `FF6_BATTLE_SPEED` (vitesse brute de la config).
- Route d'une nouvelle partie jusqu'au premier combat : copier `.scratch/drv/try14.txt` (`900 A`, `1000 A`, `10300 A`, `10694 A`, `10823-11010 U`, `11132 A`). Repères en Active (ils bougent avec les correctifs, voir `NOM.sum`) : combat lancé f11160, mosaïque f11161–11191, noir jusqu'à f11296, fondu du combat f11297–11333, temps du combat à f11341, gardes en place à f11362 ; le reste dépend du hasard et des appuis (voir `NOM.sum`). Le pilote (`FF6_AUTO_MENU`) n'appuie que si aucun message n'est affiché : il laisse traîner les menus pendant les actions. Les messages qui attendent une touche (l'expérience) demandent un appui explicite dans le script.
- `dump.cpp` lit des membres privés de `Game` (`#define private public`) : s'ils sont renommés, il ne compile plus. Lancement direct : `.scratch/drv/dump PACK SCRIPT PREMIERE DERNIERE PAS SORTIE.rgb [sons.log|-] [map:N|battle:N]`.

Cycle type pour vérifier un correctif :

```sh
cp .scratch/drv/try14.txt .scratch/drv/essai.txt
FF6_AUTO_MENU=1 bash .scratch/drv/run.sh essai 11380 11800 1 11500-14500:20 > .scratch/drv/essai.out 2>&1
cat .scratch/drv/essai.sum
python3 .scratch/vid/mix.py .scratch/cmp.png 2 3 T2195:0,60,256,80 e.scratch/drv/essai.rgb@242:0,60,256,80   # indice 242 = f11622
```

### Animations (`.scratch/anim/`)

- `hitsheet.cpp` : une ligne par script d'animation, une case par image (128 px), et la taille des images en texte.
  `clang++ -std=c++17 -O2 -Iengine/src .scratch/anim/hitsheet.cpp build/engine/libff6_game.a build/engine/libff6_core.a build/engine/libff6_audio.a -o .scratch/anim/hitsheet`
  puis `.scratch/anim/hitsheet SORTIE.ppm output/pack SCRIPT:PALETTE...` (numéros du moteur, décimaux ou `0x` : coup de garde `0x3E:0x36`, Fire Beam `331:0` bg1, `332:0` bg3, `333:0` sprite). Convertir : `python3 -c "from PIL import Image; Image.open('a.ppm').save('a.png')"`.
- `armor.cpp` (même compilation) : `armor SORTIE.ppm output/pack`, les 4 images de l'armure Magi Tek.
- `sheets.sh` + `compose.py` : les trois couches de Fire Beam, avec `build/engine/ff6 output/pack --anim 131 --layer 0|1|2 --capture f.bmp` (binaire SDL : `--target ff6`).

### Référence (`.scratch/ref/`)

- Désassemblage everything8215/ff6 au commit `813013276c952fdd27edcf7b8b86f17291542cbf` : `*.asm` = `src/battle` (logique), `btlgfx/*.asm` = `src/btlgfx` (rendu). Les plus utiles : `btlgfx/attack_anim_script.asm` (scripts d'animation, macros dans `anim_script.mac`), `anim_cmd*.asm` (interpréteur, `ExecAnimScript`), `anim_init*.asm` (fonctions d'initialisation, `CopyThread`), `gfx_cmd.asm` (`FightCmdAnim`, `MonsterDeathAnim`, chiffres), `sprite.asm`, `interrupt.asm` (NMI, copies en VRAM), `menu*.asm`, `cursor*.asm` ; côté logique `action_1.asm`, `attack.asm`, `init_*.asm`.
- Chercher avec `grep -n`, par exemple `array_label ATTACK_ANIM_SCRIPT, ATTACK_ANIM_SCRIPT::FIRE_BEAM_BG1`. Re-télécharger `btlgfx` : `bash .scratch/ref/fetch_btlgfx.sh` (URL `https://raw.githubusercontent.com/everything8215/ff6/<commit>/src/btlgfx/<fichier>` ; même schéma avec `src/battle/`).
- `numerals.mjs`, `weapons.mjs`, `guard*.mjs` : exemples de scripts Node qui lisent la ROM (`requireSupportedRom(raw).bytes`, `tools/rom.mjs`) ; monstres à `0x0F0000 + 32 × id`, `WeaponAnimProp` à `0x2CE400` (8 octets par entrée).

### Pièges

- Les `.rgb` pèsent 172 Ko par image : ne jamais les afficher ; rediriger les sorties longues vers un fichier (`> .scratch/x.out 2>&1`) puis le lire. Un run du pilote (compilation + centaines d'images) peut dépasser 2 minutes.
- Comparer les formes, les positions et les durées, pas les couleurs exactes (Ares assombrit les demi-teintes) ; `ncc.py` (corrélation des contours) mesure un décalage sans souffrir des couleurs. Le pas vertical de la vidéo (4,73 lignes de la fenêtre par ligne du jeu) peut doubler ou sauter une ligne : ± 1 pixel.
