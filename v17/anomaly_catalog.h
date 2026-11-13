#pragma once
// V17.443: immutable camera viewpoints transcribed from the user's V17.441
// screenshots. Positions in metres; yaw/pitch in degrees using camera_diagnostic.h.
// ANO-014 has no measured position, so the UI must never invent one.
#include <array>
namespace cse::anomalies {
struct Record {
    const char* id;
    const char* title;
    const char* check;
    float x,y,z,yaw,pitch,fov;
    bool cameraKnown;
    bool correctedCandidate;
};
inline constexpr std::array<Record,10> records{{
    {"ANO-005","Trois poutres devant la fresque",
     "Verifier les trois consoles flottantes au-dessus de la fresque; non corrige.",
     35.226f,2.245f,-34.273f,319.9f,34.3f,59.f,true,false},
    {"ANO-006","Escalier manquant dans le hall",
     "Verifier la liaison RDC-coursive: escalier toujours a reconstruire.",
     35.034f,2.245f,-25.576f,180.2f,11.4f,59.f,true,false},
    {"ANO-007","Vitrage incomplet jusqu'au bord",
     "Verifier le joint verre/chassis/poteau au bord de la baie.",
     49.085f,1.745f,-.605f,353.5f,-5.2f,59.f,true,false},
    {"ANO-008","Poutre decalee du mur lateral",
     "Verifier l'alignement de la poutre et du mur, sur la meme vue.",
     49.085f,1.745f,-.605f,353.5f,-5.2f,59.f,true,false},
    {"ANO-009","Palier bloque en haut de l'escalier",
     "Verifier la continuite entre l'escalier et le plancher superieur.",
     -45.121f,6.003f,-42.594f,359.2f,-10.4f,59.f,true,false},
    {"ANO-010","Rangements sous l'escalier",
     "Verifier la disparition des trois casiers et l'absence de collision.",
     -37.327f,2.245f,-35.427f,251.8f,4.2f,59.f,true,true},
    {"ANO-011","Teinte ocre des murs de piscine",
     "Verifier les murs chauds; le rendu reel reste a valider sur HD620.",
     28.763f,2.289f,-28.481f,289.2f,6.2f,59.f,true,true},
    {"ANO-012","Grand bloc marron dans la galerie",
     "Verifier la suppression du bloc strie et l'espace de passage libre.",
     -25.294f,2.245f,-11.283f,103.6f,-9.3f,59.f,true,true},
    {"ANO-013","Ombres lumineuses aliasees",
     "Verifier le crenelage des bandes lumineuses sur pilier et sol; non corrige.",
     31.133f,2.245f,-8.447f,187.9f,1.3f,59.f,true,false},
    {"ANO-014","Sauts sur les dalles exterieures",
     "Rejoindre les dallages exterieurs et marcher sur plusieurs joints; pas de position de capture connue.",
     0.f,0.f,0.f,0.f,0.f,59.f,false,true}
}};
} // namespace cse::anomalies
