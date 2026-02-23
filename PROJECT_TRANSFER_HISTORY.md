# Project Transfer History
## Oculus CV1 Full Body Tracking Control Panel | MK1

- Date de rédaction: 2026-02-23
- Contexte d’exécution: travail local dans `C:\Test\ODTKRA`
- Objectif de ce document: expliquer précisément **comment et pourquoi** ce repo est déjà avancé, pour que tu puisses publier proprement dans ton **nouveau GitHub personnel**.

---

## TL;DR
Ce projet est passé d’un exécutable legacy monolithique (ODTKRA) à une base quasi-produit avec:

- architecture modulaire (`src/core`, `src/agent`, `src/control_panel`, `tests`)
- diagnostics dry-run
- installation/MAJ TouchLink robuste
- installateur Windows (`Inno Setup`)
- pipeline CI/release
- UI Control Panel moderne et itérativement corrigée
- logs détaillés + filtres (`Latest session`, `Errors only`, `Full history`)
- branding final orienté « Oculus CV1 Full Body Tracking Control Panel »

Le tout a été fait en commits successifs dans ce repo local (pas en PR dans ton futur repo personnel).

---

## 1) Pourquoi ce fichier existe
Tu as décidé de créer un nouveau repo GitHub à toi (ce qui est une bonne idée pour repartir proprement).  
Ce document sert de **narratif de transfert**:

1. ce qui existait au départ
2. ce qui a été construit
3. les problèmes majeurs rencontrés
4. les solutions appliquées
5. l’état actuel et les points d’attention

Ainsi, quand tu publies, les gens comprennent pourquoi le code arrive déjà très mature.

---

## 2) Point de départ (état initial)
### Code legacy
Le projet initial reposait principalement sur:

- `ODTKRA/Main.cpp` (monolithe)

### Comportement initial
- logique anti-sleep centrée sur Oculus Debug Tool
- automatisation fragile de fenêtre/clavier
- dépendance forte à des timings/processus non robustes
- peu de séparation des responsabilités
- pas de vraie couche tests
- pas de UX opérable « produit »

### Risques identifiés très tôt
- parsing/états globaux fragiles
- automation UI fragile
- observabilité insuffisante
- absence de packaging industriel

---

## 3) Objectif de transformation
Le mandat de session était ambitieux: **refactor + industrialisation complète**, avec livrable installable Windows, UI de pilotage, diagnostics, install TouchLink, et expérience utilisateur solide.

Cibles principales:

- code clean/modulaire
- app agent fiable (runtime)
- control panel exploitable
- installateur simple à utiliser
- logs + diagnostics utiles
- adoption « all-in-one »

---

## 4) Architecture introduite
Nouvelle structure principale:

- `src/core`
  - config JSON + CLI
  - logger
  - diagnostics
  - anti_sleep_service
  - steam/oculus discovery
  - touchlink installer
- `src/agent/main.cpp`
  - exécution headless (`odtkra_agent.exe`)
- `src/control_panel/main.cpp`
  - UI Windows
- `tests/`
  - tests unitaires/smoke
- `packaging/odtkra.iss`
  - installateur setup .exe
- `.github/workflows/`
  - CI + release automation

Build system:

- `CMake`
- Visual Studio Build Tools
- Inno Setup

---

## 5) Chronologie détaillée des travaux (ordre logique)

## Phase A — Foundation / refactor
### A1. Base modulaire
- création du socle `src/core`
- extraction de la logique runtime en services dédiés
- ajout config persistée (`ProgramData`), logs, diagnostics

### A2. Agent runtime
- `odtkra_agent.exe` pour exécution continue / one-shot / diagnostics
- état runtime exporté via `state.json`

### A3. Tests
- ajout d’un harness test léger
- tests config/diagnostics/CLI

---

## Phase B — UI / opérations
### B1. Premier Control Panel
- Start/Stop/Restart
- diagnostics
- logs récents
- install TouchLink
- actions Steam

### B2. Observabilité
- export logs
- diagnostics dry-run détaillés
- enrichissement progressif des messages d’erreur

### B3. UX debug
- popup verbose en cas d’échec install TouchLink
- snapshot diagnostic + extrait logs

---

## Phase C — Packaging / release
### C1. Installateur
- setup Inno Setup
- install Program Files
- raccourcis menu démarrer + desktop
- désinstallation propre

### C2. CI/CD
- workflows GitHub Actions pour build/test/release
- automatisation artefacts

---

## Phase D — SteamVR / TouchLink installation
### D1. Détection SteamVR
- registry + bibliothèques Steam
- fallback sur chemin par défaut

### D2. Install/Update TouchLink
- téléchargement package source
- extraction + copie vers `SteamVR\drivers\OculusTouchLink`
- vérifications post-copy
- tentative registration `vrpathreg adddriver`

### D3. Incident majeur `(x86)`
Symptôme:
- erreurs PowerShell sur `C:\Program Files (x86)\...`

Cause:
- commande shell fragile sur chemin contenant parenthèses

Correctif:
- remplacement copie PowerShell par `std::filesystem` natif

Résultat:
- logs de succès répétés ensuite:
  - `TouchLink copied successfully with std::filesystem`
  - `vrpathreg adddriver exit=0`
  - `OculusTouchLink installed successfully`

---

## Phase E — Oculus runtime/path discovery
### E1. Path update Meta Horizon
- correction du chemin diagnostics par défaut vers Meta Horizon

### E2. Scan auto des chemins
- détection récursive de:
  - `OculusDebugToolCLI.exe`
  - `OculusClient.exe`

### E3. Réconciliation config
- fallback auto quand les chemins configurés sont invalides

---

## Phase F — Statuts runtime + branding + crédits
### F1. Statut anti-sleep synchronisé
- alignement du statut UI avec:
  - process agent
  - `state.json`
- suppression des faux positifs « active » en service arrêté

### F2. Branding installateur
- setup nommé:
  - `Oculus CV1 Full Body Tracking Control Panel`

### F3. Crédits
- ajout d’une vue Credits dans l’app
- ajout crédits dans README et messaging setup

---

## Phase G — Refonte UI et stabilisation graphique
### G1. Refonte MK1
- nouvelle hiérarchie visuelle
- sections structurées
- boutons owner-draw
- responsive layout
- DPI-aware

### G2. Série de correctifs rendering
Plusieurs itérations ont été nécessaires sur:
- ghosting
- surimpression texte
- resize artifacts
- overdraw parent/enfants

Ajustements appliqués selon itérations:
- stratégie de repaint
- clipping styles
- gestion fond/opaque/transparency
- invalidation maîtrisée
- stabilité scroll

### G3. Emphase visuelle remise
Suite feedback:
- retour des zones de surbrillance (cartes) pour améliorer la lisibilité
- accent sur la carte de statut

---

## Phase H — Logs UX avancée
### H1. Filtres logs ajoutés
- `Latest session` (défaut)
- `Errors only`
- `Full history`

### H2. Règle « latest session »
- découpage à partir du dernier marqueur:
  - `ODTKRA agent booting`

---

## 6) Problèmes rencontrés et résolution (tenants / aboutissants)

### Problème 1: Build toolchain absente
- `cmake` / `msbuild` / composants C++ manquants selon machine
- solution: installation Build Tools + CMake + Inno Setup et ajustements d’environnement

### Problème 2: Chemins Oculus non trouvés
- migration Oculus -> Meta Horizon
- solution: defaults + scan auto multi-racines

### Problème 3: Install TouchLink cassé par PowerShell `(x86)`
- solution définitive: copie native `std::filesystem`

### Problème 4: UI artefacts/ghosting
- plusieurs approches testées, puis durcissement progressif
- réduction des surimpressions via contrôle plus strict du repaint et des fonds

### Problème 5: Statuts incohérents
- solution: calcul statut basé process + state réel

---

## 7) État actuel (au moment de ce document)
### Fonctionnel
- agent runtime
- control panel opérationnel
- diagnostics
- install TouchLink fonctionne (logs récents confirment)
- setup généré localement
- filtres logs présents

### Points à surveiller
- UI rendering: zone historiquement sensible, mieux mais à valider sur ta machine cible réelle
- dépendances runtime externes (Meta/Oculus/SteamVR) toujours déterminantes

---

## 8) Historique git utile (bloc de commits modernisation)
Extraits des commits majeurs faits pendant cette session:

- `8bae7b0` refactor core
- `004ede5` UI initiale
- `c705812` CI/release/packaging docs
- `db462d1` all-in-one steam/touchlink/spacecal
- `82388f1` checks déplacés + paths Meta
- `5721534` UX debug install
- `2e0b6c4` oculus path discovery
- `f03c52b` runtime non-legacy timeout-safe
- `89d3bae` fix touchlink copy (filesystem)
- `4823cc9` branding/status/credits
- `ea789ae` MK1 UI refactor
- `86ed239` fix surimpression parent/enfant
- `42a7f3f` filtres logs
- `fcd737a` surbrillance cartes réintroduite

(Le repo contient aussi un historique plus ancien hérité des travaux précédents.)

---

## 9) Ce que ça implique pour ton nouveau GitHub
### Option A — garder l’historique complet
- tu pousses tel quel
- avantage: traçabilité complète

### Option B — repartir « clean »
- squash en 1 commit baseline
- garder ce fichier comme mémo de provenance
- avantage: historique public simplifié

---

## 10) Commandes utiles pour migration vers ton nouveau repo

```powershell
cd C:\Test\ODTKRA

git remote remove origin
# puis:
# git remote add origin <NOUVELLE_URL_GITHUB>

git push -u origin master
```

Si tu veux un squash avant push:

```powershell
# exemple (à adapter à ta stratégie)
# créer branche propre + reset/squash local avant push
```

---

## 11) Conclusion
Le projet est « quasi fini » parce que beaucoup d’itérations ont été faites d’un coup, en local, avec:

- refactor architecture
- packaging
- diagnostics
- stabilité runtime
- grosses retouches UI/UX
- corrections de bugs concrets vus en conditions réelles

Ce fichier est le **journal de transfert** officiel pour ton nouveau GitHub.

---

## Annexes
### A. Emplacement typique des artefacts locaux
- `build/`
- `packaging/output/`

### B. Setup généré localement
- `C:\Test\ODTKRA\packaging\output\ODTKRA-Setup.exe`

### C. Fichier d’origine remplacé
- `PROJECT_TRANSFER_HISTORY.txt` (remplacé par ce `.md`)
