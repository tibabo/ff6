# Rendu HD-2D : décors en 3D, personnages en 2D, sprites haute résolution

Proposition pour mêler du rendu 3D et 2D dans le moteur, à la manière du style HD-2D (Octopath Traveler, Dragon Quest III HD-2D) : des décors en trois dimensions vus en légère plongée, des personnages en sprites 2D dressés dans la scène, et la possibilité de remplacer certains sprites par des versions haute résolution. Rien de ce plan n'est implémenté : c'est l'état des lieux du moteur et l'ordre de travail recommandé.

## 1. Le rendu actuel

### Une image logicielle de 256 × 224

Tout est dessiné en logiciel dans un tampon RGBA de 256 × 224, puis ce tampon est envoyé au GPU comme une seule texture et agrandi au plus proche voisin (`engine/src/sdl/sdl_renderer.cpp`). L'interface `engine/src/renderer.hpp` ne connaît que trois opérations : créer une texture, la remplacer, la copier dans un rectangle. Il n'y a ni 3D, ni shader, ni profondeur. Les options d'affichage (8:7, couleurs d'Ares, plein écran, échelle entière) agissent sur cette seule texture.

### Chaque écran reproduit le PPU à sa manière

- **Champ** (`Game::draw`, `engine/src/game.cpp`) : les trois couches de tuiles sont rendues pixel par pixel (`field/bg.cpp`, `renderBg`), les sprites dans deux tampons (sous et sur les couches à haute priorité, `drawSprites`), puis `composeScreen` (`field/color.cpp`) applique l'ordre des couches du mode 1 et la mathématique des couleurs avec l'écran secondaire. Viennent ensuite la mosaïque, la couleur fixe, les secousses, la lampe torche (`field/stage.cpp`), la luminosité, et une seconde texture pour les dialogues et le compteur.
- **Combat** (`engine/src/battle/battle_screen.cpp`, environ 3 000 lignes) : le décor, les monstres, les personnages et les trois couches d'animation sont composés à la main dans `bgPixels_`, `objectPixels_`, `charPixels_`, `monsterPixels_` et `layerPixels_`, avec les fenêtres, les ondes HDMA, le mode 7 et l'écran secondaire, tels que les scripts d'animation du jeu les demandent (`docs/rendu-et-animations.md`). C'est la partie la plus fidèle et la plus imbriquée : la logique du déroulé et le dessin sont dans les mêmes fonctions.
- **Carte du monde et cinématiques** : un PPU logiciel complet (`cutscene/ppu.hpp`), modes 1 et 7, registres changés ligne par ligne.
- **Menus** : `menu/menu_screen.cpp`, en 256 × 224 aussi ; le combat y dessine ses fenêtres.

### Les données sont celles de la ROM

- Cartes en grilles de tuiles de 16 × 16 (`field/map.hpp`) avec, par case, les propriétés de passage et de niveau (`tileProps`), et des PNJ placés sur la grille.
- Sprites de 16 × 24 en planches d'images, recolorées par palette (BGR555) au moment du chargement (`field/sprite_data.cpp`, `buildSpriteStripColors`).
- Monstres et images d'animation en indices de palette (`data/battle_gfx.cpp`, `data/attack_anim.cpp`) : les statuts, les fondus, les éclairs, les recolorations des scripts et la mort des boss sont des opérations sur les palettes, pas sur des couleurs.

### Deux conséquences

1. **La logique parle en pixels logiques 256 × 224.** Positions des objets, déclencheurs, vecteurs des animations, fenêtres, repères des attaques : tout est dans le repère de l'écran d'origine. Ce repère ne doit jamais changer ; seul l'affichage change.
2. **Les effets reposent sur les palettes et l'écran secondaire.** Un sprite HD en vraies couleurs casserait les recolorations. Les images haute résolution doivent rester **indexées** (un indice de palette par pixel), et la correspondance indice → couleur doit se faire au moment du dessin.

## 2. Principe : un second renderer, « diorama », à côté du renderer fidèle

Le renderer logiciel reste la référence : les tests (`engine/tests`), le pilote `.scratch/drv` et les comparaisons avec `narshee-first-battle.mov` en dépendent. On ne le modifie pas ; on ajoute un chemin de rendu GPU choisi au lancement (`--render classic|hd`), en quatre étapes qui ont chacune un résultat visible.

```
                 logique du jeu (256 x 224)
                           |
                  description de scène
                   /                 \
     compositeur logiciel        renderer GPU « diorama »
     (image 256 x 224,           (résolution de la fenêtre,
      référence des tests)        3D, sprites HD, shaders)
```

## 3. Étape 0 — Décrire la scène au lieu de la peindre

Le préalable, et le gros du travail. `Game::draw` et `BattleScreen::draw` produisent des pixels ; il faut qu'ils produisent une **description de scène** consommée par deux compositeurs.

Contenu de la description :

- les couches de tuiles : carte, défilement, priorité, mosaïque, décalage par ligne ;
- les sprites : planche, image, position logique, retournements, priorité par rapport aux couches, palette et teinte, ordre de tri ;
- les couches d'animation du combat : script et image, position, fenêtre, onde, mélange de couleurs et écran secondaire, priorité de bg3 ;
- les paramètres globaux : mathématique des couleurs, couleur fixe, luminosité, secousse, fondu ;
- les fenêtres de texte et le texte lui-même (police, position), pour que la police puisse être remplacée.

Le **compositeur logiciel** consomme cette description et doit redonner **exactement** l'image actuelle. On le vérifie par des tests d'égalité pixel à pixel sur des images de référence capturées avant la refonte : le pilote (`bash .scratch/drv/run.sh`) sait déjà produire des suites d'images du champ et du combat. Tant que ces tests passent, rien n'est cassé.

Ordre : d'abord le champ, dont le dessin est déjà séparé en étapes (`renderBg`, `drawSprites`, `composeScreen`, `applyStage`). Le combat vient en dernier (étape 3) : sa description sera la plus riche, et son extraction la plus longue.

## 4. Étape 1 — Backend GPU et sprites HD indexés

### Backend

OpenGL 3.3 core via SDL2, déjà utilisé par le moteur, avec un chargeur de fonctions (glad). Sur macOS, OpenGL 4.1 est disponible mais déprécié ; si la cible inclut un jour Metal natif, la migration vers SDL3 et son API GPU est le moment de le faire. Recommandation : OpenGL pour commencer, moins de risques, shaders portables.

La résolution interne est au choix (× 2, × 4 ou celle de la fenêtre) ; les coordonnées logiques restent 256 × 224 et sont converties au dessin.

### Sprites HD indexés

- Dans le pack, un dossier optionnel `hd/` : `hd/sprites/<gfx>.png`, `hd/monsters/<id>.png`, `hd/portraits/<id>.png`, `hd/font/*.png`, à une échelle N (2, 4…), où chaque pixel est un **indice de palette** (0 à 15, 0 transparent), pas une couleur. Le format est le PNG en mode palette ou en niveaux de gris, avec l'échelle dans un petit fichier JSON à côté.
- Le shader fait la correspondance indice → couleur avec la palette du moment, transmise comme une petite texture : toutes les recolorations du jeu (statuts, fondus, éclairs, `mod_pal`, mort des boss) continuent de marcher sans qu'on y touche.
- **Sélection par sprite** : si le fichier HD manque, la planche d'origine est agrandie au plus proche voisin. On peut donc n'upscaler que certains sprites, et les ajouter un à un.
- `build-pack` ne change pas ; `hd/` est lu au lancement et ignoré par le renderer classique.

Les sprites sont dessinés comme des **panneaux dressés** (billboards), posés sur le sol au point qui correspond à leur position logique. À cette étape le jeu est encore en 2D, mais net, avec des sprites HD : un jalon utile en soi.

## 5. Étape 2 — Diorama 2.5D sans modélisation

Avec les tuiles rendues comme des quads dans une scène 3D, on obtient l'essentiel du rendu HD-2D sans modéliser un seul décor :

- caméra en légère plongée, inclinaison fixe, perspective douce ;
- les trois couches de fond posées à des profondeurs différentes : parallaxe réelle, en plus du défilement propre à chaque couche que le jeu règle (`parallax` de la carte) ;
- les tuiles marquées « au-dessus des sprites » (`topAbove`, `bottomAbove` dans les propriétés des cases) deviennent des quads relevés ou plus proches de la caméra, ce qui reproduit les priorités du PPU sans règle spéciale ;
- post-traitement : flou de profondeur (tilt-shift), bloom, grain, vignette, réglables et désactivables.

Le jeu ne change pas : collisions, déclencheurs, scripts et PNJ restent sur la grille. Les effets de mise en scène du champ (couleur fixe, mosaïque, lampe torche, secousse) deviennent des shaders de post-traitement.

## 6. Étape 3 — Décors 3D par carte, puis le combat

- **Champ** : par carte, un modèle glTF optionnel (fait dans Blender) avec une transformation tuile → unités 3D (origine, échelle, hauteur du sol par case). Le sol de marche reste la grille de la ROM : seule l'apparence change. Les cartes sans modèle gardent le diorama de l'étape 2. Chargeur : tinygltf.
- **Combat** : décor en plan 3D ou en modèle, monstres en sprites HD dressés, personnages en billboards. Les couches d'animation (rayons, boules, ondes, mode 7) restent des plans texturés : la mathématique des couleurs, les fenêtres et les ondes HDMA deviennent des opérations de fragment shader, plus naturelles qu'en logiciel. C'est l'étape la plus longue, parce que `BattleScreen` doit d'abord passer par l'étape 0.
- **Carte du monde** : le mode 7 se rend très bien comme un vrai plan en 3D ; le PPU logiciel reste pour les cinématiques.

## 7. Coûts et risques

- **L'étape 0 est la clé et la plus chère.** Le combat mêle logique et pixels ; extraire une description de scène fidèle demande plusieurs semaines, mais le travail est balisé par les tests d'égalité. Le champ est bien plus rapide.
- **Les astuces de palette** sont le piège principal : d'où les sprites HD indexés. Des sprites HD en RGB seraient plus simples à dessiner, mais il faudrait réinventer chaque recoloration.
- **Deux renderers à maintenir.** C'est voulu : le classique reste la référence pour les tests et la vidéo, le HD est la vitrine. La description de scène commune limite le doublon à la couche de dessin.
- **Dépendances nouvelles** : glad (OpenGL), tinygltf (étape 3), Blender ou un équivalent pour les décors ; aucune pour les étapes 0 et 1 côté données.
- **Performances** : un rendu logiciel à résolution interne × 4 coûterait seize fois le travail actuel par image ; c'est pourquoi la haute résolution passe par le GPU et non par un agrandissement du tampon.

## 8. Ordre recommandé

1. Étape 0 sur le champ seulement, avec les tests d'égalité.
2. Étape 1 : backend OpenGL, sprites HD indexés, sélection par fichier dans `hd/`.
3. Étape 2 : caméra inclinée et profondeur de champ, cartes de Narshe en vitrine.
4. Étape 0 puis 3 sur le combat.

Les étapes 1 et 2 donnent déjà un rendu HD-2D complet sur le champ sans toucher aux données de la ROM ni à la logique du jeu, et sans rien casser du renderer fidèle.
