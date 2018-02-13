################################################English################################################
How to use the metrics tool:
arg 1: Path to the ground truth file (Polytrack .sqlite format)
arg 2: Path to the tracker output file (Polytrack .sqlite format)
arg 3: Data association mode ABSDIST or BBOVERLAP (absolute pixel distance between centroid or bounding box overlap)
arg 4: Association threshold (a distance in pixel for ABSDIST or an overlap value between 0 and 1 for BBOVERLAP)
arg 5: Path to the tracker result file (optional)
arg 6: Put "noprompt" if you want the program to autoclose (optional), useful for scripts

Result file format:
NB_GT_TRACKS;53
NB_SYSTEM_TRACKS;76
CDT;29
FAT;38
TDF;24
TF;15
IDC;1
LT;10.4138
CTM;0.322773
CDT;29
TMEMT;43.7471
TMEMTD;15.3074
TCM;0.517275
TCD;0.00849958
MOTA;0.433373
MOTP;44.8625
MISSES_RATIO;0.400949
FP_RATIO;0.163899
MME_RATIO;0.400949


################################################Français################################################
Utilisation de l'outil de métrique:
1er argument: Chemin vers le ground truth (en format Polytrack .sqlite)
2e  argument: Chemin vers les données du tracker (en format Polytrack .sqlite)
3e  argument: Mode d'association des données ABSDIST ou BBOVERLAP (évaluation par distance de centroïd ou superposition de boîte)
4e  argument: Seuil d'association Si 3e arg est ABSDIST, alors distance en pixel. Si BBOVERLAP, alors valeur entre 0 et 1
Arguments optionnel:
5e  argument: chemin vers un fichier pour enregistrer les résultats.
6e  argument: Mettre noprompt si on veut que le programme se ferme automatiquement à la fin



Format des fichiers de résultats:
NB_GT_TRACKS;53
NB_SYSTEM_TRACKS;76
CDT;29
FAT;38
TDF;24
TF;15
IDC;1
LT;10.4138
CTM;0.322773
CDT;29
TMEMT;43.7471
TMEMTD;15.3074
TCM;0.517275
TCD;0.00849958
MOTA;0.433373
MOTP;44.8625
MISSES_RATIO;0.400949
FP_RATIO;0.163899
MME_RATIO;0.400949





