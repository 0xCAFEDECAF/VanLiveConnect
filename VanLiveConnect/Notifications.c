
#include <pgmspace.h> 

// Often used string constants
static const char PROGMEM emptyStr[] = "";

// All known notifications, as literally retrieved from my vehicle (406 year 2003, DAM number 9586; your vehicle
// may have other notification texts). Retrieving was done with the example sketch:
// "VanBus/examples/DisplayNotifications/DisplayNotifications.ino".
//
// By convention, warning notifications end with "!"; information notifications do not.

// -----
// Byte 0, 0x00...0x07

static const char msg_0_0_eng[] PROGMEM = "Tyre pressure too low!";
static const char msg_0_0_fr[] PROGMEM = "Pression pneumatique(s) insuffisante!";
static const char msg_0_0_ger[] PROGMEM = "Unzureichender Reifendruck!";
static const char msg_0_0_spa[] PROGMEM = "Presión en neumáticos insuficiente!";
static const char msg_0_0_ita[] PROGMEM = "Pressione pneumatico(ci) insufficiente!";
static const char msg_0_0_dut[] PROGMEM = "Bandenspanning te laag!";

#define msg_0_1 emptyStr

static const char msg_0_2_eng[] PROGMEM = "Automatic gearbox temperature too high!";
static const char msg_0_2_fr[] PROGMEM = "Température boîte automatique trop élevée!";
static const char msg_0_2_ger[] PROGMEM = "Temperatur im Automatikgetriebe zo hoch!";
static const char msg_0_2_spa[] PROGMEM = "Temperatura muy alta de la caja automática!";
static const char msg_0_2_ita[] PROGMEM = "Temperatura cambia automatica elevata!";
static const char msg_0_2_dut[] PROGMEM = "Temperatuur in automatische versnellingsbak te hoog!";

static const char msg_0_3_eng[] PROGMEM = "Brake fluid level low!";
static const char msg_0_3_fr[] PROGMEM = "Niveau liquide de freins insuffisant!";
static const char msg_0_3_ger[] PROGMEM = "Bremsflüssigkeitsstand zu niedrig!";
static const char msg_0_3_spa[] PROGMEM = "Nivel liquido de frenos bajo!";
static const char msg_0_3_ita[] PROGMEM = "Livello liquido freni insufficiente!";
static const char msg_0_3_dut[] PROGMEM = "Peil van remvloeistof te laag!";

static const char msg_0_4_eng[] PROGMEM = "Hydraulic suspension pressure defective!";
static const char msg_0_4_fr[] PROGMEM = "Pression suspension hydraulique déficiente!";
static const char msg_0_4_ger[] PROGMEM = "Mangelhafter Druck in der Hydraulikfederung!";
static const char msg_0_4_spa[] PROGMEM = "Presión deficiente de suspensión hidráulica!";
static const char msg_0_4_ita[] PROGMEM = "Pressione sospensione idraulica insufficiente!";
static const char msg_0_4_dut[] PROGMEM = "Hydraulische drukophanging defekt!";  // [sic]

static const char msg_0_5_eng[] PROGMEM = "Suspension defective!";
static const char msg_0_5_fr[] PROGMEM = "Suspension défaillante!";
static const char msg_0_5_ger[] PROGMEM = "Federung defekt!";
static const char msg_0_5_spa[] PROGMEM = "Mala suspensión!";
static const char msg_0_5_ita[] PROGMEM = "Sospensione in panne!";
static const char msg_0_5_dut[] PROGMEM = "Ophanging defekt!";

static const char msg_0_6_eng[] PROGMEM = "Engine oil temperature too high!";
static const char msg_0_6_fr[] PROGMEM = "Température huile moteur trop élevée!";
static const char msg_0_6_ger[] PROGMEM = "Temperatur Motoröl zu hoch!";
static const char msg_0_6_spa[] PROGMEM = "Temperatura muy alta de aceite motor!";
static const char msg_0_6_ita[] PROGMEM = "Temperatura olio motore troppo elevata!";
static const char msg_0_6_dut[] PROGMEM = "Temperatuur motorolie te hoog!";

static const char msg_0_7_eng[] PROGMEM = "Engine temperature too high!";
static const char msg_0_7_fr[] PROGMEM = "Température moteur excessive!";
static const char msg_0_7_ger[] PROGMEM = "Stark überhöhte Motortemperatur!";
static const char msg_0_7_spa[] PROGMEM = "Temperatura excesiva de motor!";
static const char msg_0_7_ita[] PROGMEM = "Temperatura motore eccessiva!";
static const char msg_0_7_dut[] PROGMEM = "Te hoge motortemperatuur!";

// -----
// Byte 1, 0x08...0x0F

static const char msg_1_0_eng[] PROGMEM = "Clear diesel filter (FAP) URGENT";
static const char msg_1_0_fr[] PROGMEM = "Cycle de nettoyage du filtre diesel (FAP) à faire rapidement";
static const char msg_1_0_ger[] PROGMEM = "Die Reinigung des Diesel-Filters (FAP) ist dringend erforderlich";
static const char msg_1_0_spa[] PROGMEM = "Hacer pronto la limpieza del filtro diesel (FAP)";
static const char msg_1_0_ita[] PROGMEM = "Pulire il filtro diesel (FAP) al più presto";
static const char msg_1_0_dut[] PROGMEM = "Dieselfilter snel schoonmaken";

#define msg_1_1 emptyStr

static const char msg_1_2_eng[] PROGMEM = "Min level additive gasoil!";
static const char msg_1_2_fr[] PROGMEM = "Niveau mini additif gasoil!";
static const char msg_1_2_ger[] PROGMEM = "Stand der Krafstoff-Additif ist zu nierdrig!";  // [sic, 2 x]
static const char msg_1_2_spa[] PROGMEM = "Nivel gasoil al minimo!";  // incorrect, should mention "additive"
static const char msg_1_2_ita[] PROGMEM = "Livello minimo gasolio!";  // incorrect, should mention "additive"
static const char msg_1_2_dut[] PROGMEM = "Minimum niveau aanvullende vloeistof diesel!";  // "aanvullende" is incorrect

static const char msg_1_3_eng[] PROGMEM = "Fuel cap open!";
static const char msg_1_3_fr[] PROGMEM = "Accès réservoir carburant mal verrouillé!";
static const char msg_1_3_ger[] PROGMEM = "Tankdeckel offen!";
static const char msg_1_3_spa[] PROGMEM = "Tapa abierta!";
static const char msg_1_3_ita[] PROGMEM = "Tappa aperto!";
static const char msg_1_3_dut[] PROGMEM = "Tankdop open!";

static const char msg_1_4_eng[] PROGMEM = "Puncture(s) detected!";
static const char msg_1_4_fr[] PROGMEM = "Roue(s) crevée(s) détectée(s)!";
static const char msg_1_4_ger[] PROGMEM = "Reifenpanne festgestellt!";
static const char msg_1_4_spa[] PROGMEM = "Rueda(s) pinchada(s) detectada(s)!";
static const char msg_1_4_ita[] PROGMEM = "Presenza di ruota(e) forata(e)!";
static const char msg_1_4_dut[] PROGMEM = "Lekke band(en)!";

static const char msg_1_5_eng[] PROGMEM = "Cooling circuit level too low!";
static const char msg_1_5_fr[] PROGMEM = "Niveau circuit de refroidissement insuffisant!";
static const char msg_1_5_ger[] PROGMEM = "Unzureichender Pegel des Kühlkreises!";
static const char msg_1_5_spa[] PROGMEM = "Nivel bajo del circuito de refrigeración!";
static const char msg_1_5_ita[] PROGMEM = "Livello circuito di raffreddamento insufficiente!";
static const char msg_1_5_dut[] PROGMEM = "Koelwaterpeil te laag!";

static const char msg_1_6_eng[] PROGMEM = "Oil pressure insufficient!";
static const char msg_1_6_fr[] PROGMEM = "Pression huile moteur insuffisante!";
static const char msg_1_6_ger[] PROGMEM = "Motoröldruck zu niedrig!";
static const char msg_1_6_spa[] PROGMEM = "Presion aceite motor insuficiente!";
static const char msg_1_6_ita[] PROGMEM = "Pressione olio motore insufficiente!";
static const char msg_1_6_dut[] PROGMEM = "Motoroliedruk te laag!";

static const char msg_1_7_eng[] PROGMEM = "Engine oil level too low!";
static const char msg_1_7_fr[] PROGMEM = "Niveau d'huile moteur insuffisant!";
static const char msg_1_7_ger[] PROGMEM = "Unzureichender Motorölstand!";
static const char msg_1_7_spa[] PROGMEM = "Nivel insuficiente del aceite motor!";
static const char msg_1_7_ita[] PROGMEM = "Livello olio motore insufficiente!";
static const char msg_1_7_dut[] PROGMEM = "Motoroliepeil te laag!";

// -----
// Byte 2, 0x10...0x17

static const char msg_2_0_eng[] PROGMEM = "Engine antipollution system defective!";
static const char msg_2_0_fr[] PROGMEM = "Système antipollution moteur déficient!";
static const char msg_2_0_ger[] PROGMEM = "Umweltschutzsystem Motor schwach!";
static const char msg_2_0_spa[] PROGMEM = "Sistema antipolución motor deficiente!";
static const char msg_2_0_ita[] PROGMEM = "Sistema antinquinamento motore in panne!";
static const char msg_2_0_dut[] PROGMEM = "Antivervuilingssysteem van motor defekt!";

static const char msg_2_1_eng[] PROGMEM = "Brake pads worn!";
static const char msg_2_1_fr[] PROGMEM = "Plaquettes de freins usées!";
static const char msg_2_1_ger[] PROGMEM = "Bremsbeläge abgenutzt!";
static const char msg_2_1_spa[] PROGMEM = "Plaquetas de freno gastadas!";
static const char msg_2_1_ita[] PROGMEM = "Pastiglie dei freni usate!";
static const char msg_2_1_dut[] PROGMEM = "Remblokken versleten!";

static const char msg_2_2_eng[] PROGMEM = "Check Control OK";
#define msg_2_2_fr msg_2_2_eng
#define msg_2_2_ger msg_2_2_eng
#define msg_2_2_spa msg_2_2_eng
#define msg_2_2_ita msg_2_2_eng
#define msg_2_2_dut msg_2_2_eng

static const char msg_2_3_eng[] PROGMEM = "Automatic gearbox defective!";
static const char msg_2_3_fr[] PROGMEM = "Boîte automatique défaillante!";
static const char msg_2_3_ger[] PROGMEM = "Automatikgetriebe defekt!";
static const char msg_2_3_spa[] PROGMEM = "Caja automática defectuosa!";
static const char msg_2_3_ita[] PROGMEM = "Cambio automatico in panne!";
static const char msg_2_3_dut[] PROGMEM = "Automatische versnellingsbak defekt!";

static const char msg_2_4_eng[] PROGMEM = "ASR / ESP system defective!";
static const char msg_2_4_fr[] PROGMEM = "Système ASR / ESP défaillant!";
static const char msg_2_4_ger[] PROGMEM = "ASR / ESP System defekt!";
static const char msg_2_4_spa[] PROGMEM = "Sistema ASR / ESP defectuoso!";
static const char msg_2_4_ita[] PROGMEM = "Sistema ASR / ESP in panne!";
static const char msg_2_4_dut[] PROGMEM = "ASR/ESP-systeem defekt!";

static const char msg_2_5_eng[] PROGMEM = "ABS brake system defective!";
static const char msg_2_5_fr[] PROGMEM = "Système de freinage ABS défaillant!";
static const char msg_2_5_ger[] PROGMEM = "ABS Bremssystem defekt!";
static const char msg_2_5_spa[] PROGMEM = "Sistema de freno ABS defectuoso!";
static const char msg_2_5_ita[] PROGMEM = "Sistema di frenatura ABS in panne!";
static const char msg_2_5_dut[] PROGMEM = "ABS-remsysteem defekt!";

static const char msg_2_6_eng[] PROGMEM = "Suspension and power steering defective!";
static const char msg_2_6_fr[] PROGMEM = "Suspension et direction assistée défaillantes!";
static const char msg_2_6_ger[] PROGMEM = "Federung und Servolenkung defekt!";
static const char msg_2_6_spa[] PROGMEM = "Suspensión y dirección asistida defectuosa!";
static const char msg_2_6_ita[] PROGMEM = "Sospensione e servosterzo in panne!";
static const char msg_2_6_dut[] PROGMEM = "Ophanging en stuurbekrachtiging defekt!";

static const char msg_2_7_eng[] PROGMEM = "Brake system defective!";
static const char msg_2_7_fr[] PROGMEM = "Système de freinage défaillant!";
static const char msg_2_7_ger[] PROGMEM = "Bremssystem defekt!";
static const char msg_2_7_spa[] PROGMEM = "Sistema de frenado defectuoso!";
static const char msg_2_7_ita[] PROGMEM = "Sistema di frenatura in panne!";
static const char msg_2_7_dut[] PROGMEM = "Remsysteem defekt!";

// -----
// Byte 3, 0x18...0x1F

static const char msg_3_0_eng[] PROGMEM = "Airbag defective!";
static const char msg_3_0_fr[] PROGMEM = "Airbag défaillant!";
static const char msg_3_0_ger[] PROGMEM = "Airbag defekt!";
static const char msg_3_0_spa[] PROGMEM = "Airbag defectuoso!";
static const char msg_3_0_ita[] PROGMEM = "Airbag in panne!";
static const char msg_3_0_dut[] PROGMEM = "Airbag defekt!";

static const char msg_3_1_eng[] PROGMEM = "Airbag defective!";
static const char msg_3_1_fr[] PROGMEM = "Airbag défaillant!";
static const char msg_3_1_ger[] PROGMEM = "Airbag defekt!";
static const char msg_3_1_spa[] PROGMEM = "Airbag defectuoso!";
static const char msg_3_1_ita[] PROGMEM = "Airbag in panne!";
static const char msg_3_1_dut[] PROGMEM = "Airbag defekt!";

#define msg_3_2 emptyStr

static const char msg_3_3_eng[] PROGMEM = "Engine temperature high!";
static const char msg_3_3_fr[] PROGMEM = "Température moteur élevée!";
static const char msg_3_3_ger[] PROGMEM = "Hohe Motortemperatur!";
static const char msg_3_3_spa[] PROGMEM = "Temperatura alta del motor!";
static const char msg_3_3_ita[] PROGMEM = "Temperatura motore elevata!";
static const char msg_3_3_dut[] PROGMEM = "Motortemperatuur hoog!";

#define msg_3_4 emptyStr

#define msg_3_5 emptyStr

#define msg_3_6 emptyStr

static const char msg_3_7_eng[] PROGMEM = "Water in Diesel fuel filter";
static const char msg_3_7_fr[] PROGMEM = "Présence d'eau dans le filtre à gasoil";
static const char msg_3_7_ger[] PROGMEM = "Wasser im Dieselfilter";
static const char msg_3_7_spa[] PROGMEM = "Presencia de agua en el filtro de gasoil";
static const char msg_3_7_ita[] PROGMEM = "Presenza d'acqua nel filtro del gasolio";
static const char msg_3_7_dut[] PROGMEM = "Water aanwezig in dieselfilter";

// -----
// Byte 4, 0x20...0x27

#define msg_4_0 emptyStr

static const char msg_4_1_eng[] PROGMEM = "Automatic beam adjustment defective!";
static const char msg_4_1_fr[] PROGMEM = "Réglage automatique des projecteurs défaillant!";
static const char msg_4_1_ger[] PROGMEM = "Automatische Scheinwerfereinstellung defekt!";
static const char msg_4_1_spa[] PROGMEM = "Ajuste automático de proyectores defectuosos!";
static const char msg_4_1_ita[] PROGMEM = "Regolazione automatica dei fari in panne!";
static const char msg_4_1_dut[] PROGMEM = "Automatische koplampinstelling defekt!";

#define msg_4_2 emptyStr

#define msg_4_3 emptyStr

static const char msg_4_4_eng[] PROGMEM = "Service battery charge low!";
static const char msg_4_4_fr[] PROGMEM = "Batterie de service faible!";
static const char msg_4_4_ger[] PROGMEM = "Hilfsbatterie schwach!";
static const char msg_4_4_spa[] PROGMEM = "Batería de servicio baja de nivel!";
static const char msg_4_4_ita[] PROGMEM = "Batteria di servizio debole!";
static const char msg_4_4_dut[] PROGMEM = "Extra accu zwak!";

static const char msg_4_5_eng[] PROGMEM = "Battery charge low!";
static const char msg_4_5_fr[] PROGMEM = "Charge batterie déficiente!";
static const char msg_4_5_ger[] PROGMEM = "Mangelhafte Batterieladung!";
static const char msg_4_5_spa[] PROGMEM = "Carga batería deficiente!";
static const char msg_4_5_ita[] PROGMEM = "Carica della batteria insufficiente!";
static const char msg_4_5_dut[] PROGMEM = "Acculading te laag!";

static const char msg_4_6_eng[] PROGMEM = "Diesel antipollution system (FAP) defective!";
static const char msg_4_6_fr[] PROGMEM = "Système antipollution diesel (FAP) défaillant!";
static const char msg_4_6_ger[] PROGMEM = "Umweltschutzsystem Diesel (FAP) defekt!";
static const char msg_4_6_spa[] PROGMEM = "Sistema antipolución gasoil (FAP) defectuoso!";
static const char msg_4_6_ita[] PROGMEM = "Sistema antinquinamento diesel (FAP) in panne!";
static const char msg_4_6_dut[] PROGMEM = "Antivervuilingssysteem dieselmotor (FAP) defekt!";

static const char msg_4_7_eng[] PROGMEM = "Engine antipollution system inoperative!";
static const char msg_4_7_fr[] PROGMEM = "Système antipollution moteur inopérant!";
static const char msg_4_7_ger[] PROGMEM = "Umweltschutzsystem Motor wirkungslos!";
static const char msg_4_7_spa[] PROGMEM = "Sistema antiploución motor fuera de funcionamiento!";
static const char msg_4_7_ita[] PROGMEM = "Sistema antinquinamento motore inoperante!";
static const char msg_4_7_dut[] PROGMEM = "Antivervuilingssysteem motor defekt!";

// -----
// Byte 5, 0x28...0x2F

static const char msg_5_0_eng[] PROGMEM = "Handbrake on!";
static const char msg_5_0_fr[] PROGMEM = "Oubli frein à main!";
static const char msg_5_0_ger[] PROGMEM = "Handbremse lösen!";
static const char msg_5_0_spa[] PROGMEM = "Olvido freno parking!";
static const char msg_5_0_ita[] PROGMEM = "Freno a mano ancora inserito!";
static const char msg_5_0_dut[] PROGMEM = "Handrem !!";

static const char msg_5_1_eng[] PROGMEM = "Safety belt not fastened!";
static const char msg_5_1_fr[] PROGMEM = "Oubli ceinture de sécurité!";
static const char msg_5_1_ger[] PROGMEM = "Sicherheitsgurt vergessen!";
static const char msg_5_1_spa[] PROGMEM = "Cinturón de seguridad olvidado!";
static const char msg_5_1_ita[] PROGMEM = "Cintura di sicurezza non agganciata!";
static const char msg_5_1_dut[] PROGMEM = "Veiligheidsgordel !!";

static const char msg_5_2_eng[] PROGMEM = "Passenger airbag neutralized";
static const char msg_5_2_fr[] PROGMEM = "Airbag passager neutralisé";
static const char msg_5_2_ger[] PROGMEM = "Beifahrerairbag deaktiviert";
static const char msg_5_2_spa[] PROGMEM = "Airbag pasajero desactivado";
static const char msg_5_2_ita[] PROGMEM = "Airbag passaggero neutralizzato";
static const char msg_5_2_dut[] PROGMEM = "Passagiersairbag buiten werking";

static const char msg_5_3_eng[] PROGMEM = "Windshield liquid level too low";
static const char msg_5_3_fr[] PROGMEM = "Niveau liquide lave glace insuffisant";
static const char msg_5_3_ger[] PROGMEM = "Unzureichender Wischwasserstand";
static const char msg_5_3_spa[] PROGMEM = "Nivel bajo del líquido limpiaparabrisas";
static const char msg_5_3_ita[] PROGMEM = "Livello del liquido tergicristalli insufficiente";
static const char msg_5_3_dut[] PROGMEM = "Peil ruitenwisservloeistof te laag";

static const char msg_5_4_eng[] PROGMEM = "Current speed too high";
static const char msg_5_4_fr[] PROGMEM = "Vitesse actuelle excessive";
static const char msg_5_4_ger[] PROGMEM = "Derzeitige Geschwindigkeit überhöht";
static const char msg_5_4_spa[] PROGMEM = "Velocidad actual excesiva";
static const char msg_5_4_ita[] PROGMEM = "Velocità attuale eccessiva";
static const char msg_5_4_dut[] PROGMEM = "Huidige snelheid te hoog";

static const char msg_5_5_eng[] PROGMEM = "Ignition key still inserted";
static const char msg_5_5_fr[] PROGMEM = "Oubli clé de contact";
static const char msg_5_5_ger[] PROGMEM = "Zündschlüssel vergessen";
static const char msg_5_5_spa[] PROGMEM = "Llave de contacto olvidada";
static const char msg_5_5_ita[] PROGMEM = "Chiave di contatto dimenticata";
static const char msg_5_5_dut[] PROGMEM = "Contactsleutel ! ";

static const char msg_5_6_eng[] PROGMEM = "Lights not on";
static const char msg_5_6_fr[] PROGMEM = "Oubli feux de position";
static const char msg_5_6_ger[] PROGMEM = "Standlicht vergessen";
static const char msg_5_6_spa[] PROGMEM = "Luces olvidadas";
static const char msg_5_6_ita[] PROGMEM = "Fari accesi";
static const char msg_5_6_dut[] PROGMEM = "Parkeerlichten ! ";

#define msg_5_7 emptyStr

// -----
// Byte 6, 0x30...0x37

static const char msg_6_0_eng[] PROGMEM = "Impact sensor defective";
static const char msg_6_0_fr[] PROGMEM = "Capteur de choc défaillant";
static const char msg_6_0_ger[] PROGMEM = "Aufprallfühler defekt";
static const char msg_6_0_spa[] PROGMEM = "Detector de choque defectuoso";
static const char msg_6_0_ita[] PROGMEM = "Sensore die choc in panne";
static const char msg_6_0_dut[] PROGMEM = "Schoksensor defekt";

#define msg_6_1 emptyStr

static const char msg_6_2_eng[] PROGMEM = "Tyre pressure sensor battery low";
static const char msg_6_2_fr[] PROGMEM = "Pile capteur pression pneumatique usagée";
static const char msg_6_2_ger[] PROGMEM = "Batterie des Reifendruckfühlers verbraucht";
static const char msg_6_2_spa[] PROGMEM = "Pila del captador de presión neumática gastada";
static const char msg_6_2_ita[] PROGMEM = "Pila del sensore di pressione del pneumatico scarica";
static const char msg_6_2_dut[] PROGMEM = "Batterij van bandendruk-sensor leeg";

static const char msg_6_3_eng[] PROGMEM = "Plip remote control battery low";
static const char msg_6_3_fr[] PROGMEM = "Pile télécommande plip usagée";
static const char msg_6_3_ger[] PROGMEM = "Batterie der Fernbedienung verbraucht";
static const char msg_6_3_spa[] PROGMEM = "Pila del control remoto gastada";
static const char msg_6_3_ita[] PROGMEM = "Pila del telecomando porte scarica";
static const char msg_6_3_dut[] PROGMEM = "Batterij van plip-afstandsbediending leeg";

#define msg_6_4 emptyStr

static const char msg_6_5_eng[] PROGMEM = "Place automatic gearbox in P position";
static const char msg_6_5_fr[] PROGMEM = "Placer boîte automatique en position P";
static const char msg_6_5_ger[] PROGMEM = "Automatikgetriebe auf P stellen";
static const char msg_6_5_spa[] PROGMEM = "Poner caja automática en posición P";
static const char msg_6_5_ita[] PROGMEM = "Mettere il cambio automatica in posizione P";
static const char msg_6_5_dut[] PROGMEM = "Automatische versnelling in P-stand";

static const char msg_6_6_eng[] PROGMEM = "Testing stop lamps : brake gently";
static const char msg_6_6_fr[] PROGMEM = "Test lampes stop : freinez légèrement";
static const char msg_6_6_ger[] PROGMEM = "Test der Bremslichter: leicht bremsen";
static const char msg_6_6_spa[] PROGMEM = "Test de luces de parada: frenar levemente";
static const char msg_6_6_ita[] PROGMEM = "Test delle luci de stop : frenare leggermente";
static const char msg_6_6_dut[] PROGMEM = "Test remlichten : druk licht op de rem";

static const char msg_6_7_eng[] PROGMEM = "Fuel level low!";
static const char msg_6_7_fr[] PROGMEM = "Niveau carburant faible!";
static const char msg_6_7_ger[] PROGMEM = "Kraftstoffstand niedrig!";
static const char msg_6_7_spa[] PROGMEM = "Nivel carburante bajo!";
static const char msg_6_7_ita[] PROGMEM = "Livello carburante basso!";
static const char msg_6_7_dut[] PROGMEM = "Brandstofpeil laag!";

// -----
// Byte 7, 0x38...0x3F

static const char msg_7_0_eng[] PROGMEM = "Automatic headlight activation system disabled";
static const char msg_7_0_fr[] PROGMEM = "Allumage automatique des projecteurs désactivé";
static const char msg_7_0_ger[] PROGMEM = "Automatisches Einschalten der Scheinwerfer deaktiviert";
static const char msg_7_0_spa[] PROGMEM = "Encendido automático de proyectores desactivado";
static const char msg_7_0_ita[] PROGMEM = "Accensione automatica dei fari disattivata";
static const char msg_7_0_dut[] PROGMEM = "Automatische koplampschakeling uit";

static const char msg_7_1_eng[] PROGMEM = "Turn-headlight defective!";
static const char msg_7_1_fr[] PROGMEM = "Défaut codes virages!";
static const char msg_7_1_ger[] PROGMEM = "Neben-Abblendlicht defekt!";
static const char msg_7_1_spa[] PROGMEM = "Defecto códigos giros!";
static const char msg_7_1_ita[] PROGMEM = "Difetto antiabbagliante per curva!";
static const char msg_7_1_dut[] PROGMEM = "Bochtenverlichting defekt!";

static const char msg_7_2_eng[] PROGMEM = "Turn-headlight disable";
static const char msg_7_2_fr[] PROGMEM = "Codes virages inactifs";
static const char msg_7_2_ger[] PROGMEM = "Neben-Abblendlicht deaktiviert";
static const char msg_7_2_spa[] PROGMEM = "Códigos giros inactivos";
static const char msg_7_2_ita[] PROGMEM = "Antiabbagliante per curva inattivo";
static const char msg_7_2_dut[] PROGMEM = "Bochtenverlichting buiten werking";

static const char msg_7_3_eng[] PROGMEM = "Turn-headlight enable";
static const char msg_7_3_fr[] PROGMEM = "Codes virages actifs";
static const char msg_7_3_ger[] PROGMEM = "Neben-Abblendlicht aktiviert";
static const char msg_7_3_spa[] PROGMEM = "Códigos giros activos";
static const char msg_7_3_ita[] PROGMEM = "Antiabbagliante per curva attivo";
static const char msg_7_3_dut[] PROGMEM = "Bochtenverlichting aan";

#define msg_7_4 emptyStr

static const char msg_7_5_eng[] PROGMEM = "7 tyre pressure sensors missing!";
static const char msg_7_5_fr[] PROGMEM = "7 capteurs pression pneumatique manquants!";
static const char msg_7_5_ger[] PROGMEM = "7 Reifendruckfühler fehlen!";
static const char msg_7_5_spa[] PROGMEM = "Faltan 7 captadores de presión neumática!";
static const char msg_7_5_ita[] PROGMEM = "Mancano 7 sensori di pressione pneumatici!";
static const char msg_7_5_dut[] PROGMEM = "Er ontbreken 7 bandendruk-sensors!";

#define msg_7_6_eng msg_7_5_eng
#define msg_7_6_fr msg_7_5_fr
#define msg_7_6_ger msg_7_5_ger
#define msg_7_6_spa msg_7_5_spa
#define msg_7_6_ita msg_7_5_ita
#define msg_7_6_dut msg_7_5_dut

#define msg_7_7_eng msg_7_5_eng
#define msg_7_7_fr msg_7_5_fr
#define msg_7_7_ger msg_7_5_ger
#define msg_7_7_spa msg_7_5_spa
#define msg_7_7_ita msg_7_5_ita
#define msg_7_7_dut msg_7_5_dut

// -----
// Byte 8, 0x40...0x47

static const char msg_8_0_eng[] PROGMEM = "Doors locked";
static const char msg_8_0_fr[] PROGMEM = "Portes verrouillées";
static const char msg_8_0_ger[] PROGMEM = "Türen verriegelt";
static const char msg_8_0_spa[] PROGMEM = "Puertas condenadas";
static const char msg_8_0_ita[] PROGMEM = "Porte bloccate";
static const char msg_8_0_dut[] PROGMEM = "Deuren vergrendeld";

static const char msg_8_1_eng[] PROGMEM = "ASR / ESP system disabled";
static const char msg_8_1_fr[] PROGMEM = "Système ASR / ESP désactivé";
static const char msg_8_1_ger[] PROGMEM = "ASR / ESP System deaktiviert";
static const char msg_8_1_spa[] PROGMEM = "Sistema ASR / ESP fuera de servicio";
static const char msg_8_1_ita[] PROGMEM = "Sistema ASR / ESP disattivato";
static const char msg_8_1_dut[] PROGMEM = "ASR/ESP-systeem uit";

static const char msg_8_2_eng[] PROGMEM = "Child safety lock enabled";
static const char msg_8_2_fr[] PROGMEM = "Sécurité enfant activée";
static const char msg_8_2_ger[] PROGMEM = "Kindersicherung aktiviert";
static const char msg_8_2_spa[] PROGMEM = "Seguridad niños activa";
static const char msg_8_2_ita[] PROGMEM = "Sicurezza bambini attivata";
static const char msg_8_2_dut[] PROGMEM = "Kindervergrendeling aan";

static const char msg_8_3_eng[] PROGMEM = "Door self locking system enabled";
static const char msg_8_3_fr[] PROGMEM = "Autoverrouillage des portes activé";
static const char msg_8_3_ger[] PROGMEM = "Selbstverriegelung der Türen aktiviert";
static const char msg_8_3_spa[] PROGMEM = "Autobloqueo de puertas activo";
static const char msg_8_3_ita[] PROGMEM = "Auto bloccaggio delle porte attivato";
static const char msg_8_3_dut[] PROGMEM = "Automatische deurvergrendling aan";

static const char msg_8_4_eng[] PROGMEM = "Automatic headlight activation system enabled";
static const char msg_8_4_fr[] PROGMEM = "Allumage automatique des projecteurs activé";
static const char msg_8_4_ger[] PROGMEM = "Automatisches Einschalten der Scheinwerfer aktiviert";
static const char msg_8_4_spa[] PROGMEM = "Encendido automático de proyectores activo";
static const char msg_8_4_ita[] PROGMEM = "Accensione automatica dei fari attivata";
static const char msg_8_4_dut[] PROGMEM = "Automatische koplampschakeling aan";

static const char msg_8_5_eng[] PROGMEM = "Automatic wiper system enabled";
static const char msg_8_5_fr[] PROGMEM = "Essuie-vitre automatique activé";
static const char msg_8_5_ger[] PROGMEM = "Automatischer Scheibenwischer aktiviert";
static const char msg_8_5_spa[] PROGMEM = "Limpiaparabrisas activo";
static const char msg_8_5_ita[] PROGMEM = "Tergicristalli automatici attivati";
static const char msg_8_5_dut[] PROGMEM = "Automatische ruitenwisser aan";

static const char msg_8_6_eng[] PROGMEM = "Electronic anti-theft system defective";
static const char msg_8_6_fr[] PROGMEM = "Antivol électronique défaillant";
static const char msg_8_6_ger[] PROGMEM = "Elektronische Diebstahlsicherung defekt";
static const char msg_8_6_spa[] PROGMEM = "Antirrobo electronico defectuoso";
static const char msg_8_6_ita[] PROGMEM = "Antifurto elettronico in panne";
static const char msg_8_6_dut[] PROGMEM = "Elektronische diefstalbeveiliging defekt";

static const char msg_8_7_eng[] PROGMEM = "Sport suspension mode enabled";
static const char msg_8_7_fr[] PROGMEM = "Mode suspension sport activé";
static const char msg_8_7_ger[] PROGMEM = "Sportfederung aktiviert";
static const char msg_8_7_spa[] PROGMEM = "Modo suspensión sport activo";
static const char msg_8_7_ita[] PROGMEM = "Modo sospensione sport attivato";
static const char msg_8_7_dut[] PROGMEM = "Sportief ophangingssysteem aan";

// -----
// Byte 9 is the index of the current message

// -----
// Byte 10, 0x50...0x57

#define msg_10_0 emptyStr
#define msg_10_1 emptyStr
#define msg_10_2 emptyStr
#define msg_10_3 emptyStr
#define msg_10_4 emptyStr
#define msg_10_5 emptyStr
#define msg_10_6 emptyStr
#define msg_10_7 emptyStr

// -----
// Byte 11, 0x58...0x5F

#define msg_11_0 emptyStr
#define msg_11_1 emptyStr
#define msg_11_2 emptyStr
#define msg_11_3 emptyStr
#define msg_11_4 emptyStr
#define msg_11_5 emptyStr
#define msg_11_6 emptyStr
#define msg_11_7 emptyStr

// -----
// Byte 12, 0x60...0x67

#define msg_12_0 emptyStr
#define msg_12_1 emptyStr
#define msg_12_2 emptyStr
#define msg_12_3 emptyStr
#define msg_12_4 emptyStr
#define msg_12_5 emptyStr
#define msg_12_6 emptyStr
#define msg_12_7 emptyStr

// -----
// Byte 13, 0x68...0x6F

#define msg_13_0 emptyStr
#define msg_13_1 emptyStr
#define msg_13_2 emptyStr
#define msg_13_3 emptyStr
#define msg_13_4 emptyStr
#define msg_13_5 emptyStr
#define msg_13_6 emptyStr
#define msg_13_7 emptyStr

// On vehicles made after 2004

// -----
// Byte 14, 0x70...0x77

#define msg_14_0 emptyStr
#define msg_14_1 emptyStr
#define msg_14_2 emptyStr
#define msg_14_3 emptyStr
#define msg_14_4 emptyStr
#define msg_14_5 emptyStr
#define msg_14_6 emptyStr
#define msg_14_7 emptyStr

// -----
// Byte 15, 0x78...0x7F

#define msg_15_0 emptyStr
#define msg_15_1 emptyStr
#define msg_15_2 emptyStr
#define msg_15_3 emptyStr
#define msg_15_4 emptyStr
#define msg_15_5 emptyStr
#define msg_15_6 emptyStr
#define msg_15_7 emptyStr

// To save precious RAM, we use a PROGMEM array of PROGMEM strings
// See also: https://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html
const char *const msgTable_eng[] PROGMEM =
{
    msg_0_0_eng, msg_0_1    , msg_0_2_eng, msg_0_3_eng, msg_0_4_eng, msg_0_5_eng, msg_0_6_eng, msg_0_7_eng,
    msg_1_0_eng, msg_1_1    , msg_1_2_eng, msg_1_3_eng, msg_1_4_eng, msg_1_5_eng, msg_1_6_eng, msg_1_7_eng,
    msg_2_0_eng, msg_2_1_eng, msg_2_2_eng, msg_2_3_eng, msg_2_4_eng, msg_2_5_eng, msg_2_6_eng, msg_2_7_eng,
    msg_3_0_eng, msg_3_1_eng, msg_3_2    , msg_3_3_eng, msg_3_4    , msg_3_5    , msg_3_6    , msg_3_7_eng,
    msg_4_0    , msg_4_1_eng, msg_4_2    , msg_4_3    , msg_4_4_eng, msg_4_5_eng, msg_4_6_eng, msg_4_7_eng,
    msg_5_0_eng, msg_5_1_eng, msg_5_2_eng, msg_5_3_eng, msg_5_4_eng, msg_5_5_eng, msg_5_6_eng, msg_5_7    ,
    msg_6_0_eng, msg_6_1    , msg_6_2_eng, msg_6_3_eng, msg_6_4    , msg_6_5_eng, msg_6_6_eng, msg_6_7_eng,
    msg_7_0_eng, msg_7_1_eng, msg_7_2_eng, msg_7_3_eng, msg_7_4    , msg_7_5_eng, msg_7_6_eng, msg_7_7_eng,
    msg_8_0_eng, msg_8_1_eng, msg_8_2_eng, msg_8_3_eng, msg_8_4_eng, msg_8_5_eng, msg_8_6_eng, msg_8_7_eng,
    0, 0, 0, 0, 0, 0, 0, 0,
    msg_10_0, msg_10_1, msg_10_2, msg_10_3, msg_10_4, msg_10_5, msg_10_6, msg_10_7,
    msg_11_0, msg_11_1, msg_11_2, msg_11_3, msg_11_4, msg_11_5, msg_11_6, msg_11_7,
    msg_12_0, msg_12_1, msg_12_2, msg_12_3, msg_12_4, msg_12_5, msg_12_6, msg_12_7,
    msg_13_0, msg_13_1, msg_13_2, msg_13_3, msg_13_4, msg_13_5, msg_13_6, msg_13_7,
    msg_14_0, msg_14_1, msg_14_2, msg_14_3, msg_14_4, msg_14_5, msg_14_6, msg_14_7,
    msg_15_0, msg_15_1, msg_15_2, msg_15_3, msg_15_4, msg_15_5, msg_15_6, msg_15_7
};

const char *const msgTable_fr[] PROGMEM =
{
    msg_0_0_fr , msg_0_1    , msg_0_2_fr , msg_0_3_fr , msg_0_4_fr , msg_0_5_fr , msg_0_6_fr , msg_0_7_fr ,
    msg_1_0_fr , msg_1_1    , msg_1_2_fr , msg_1_3_fr , msg_1_4_fr , msg_1_5_fr , msg_1_6_fr , msg_1_7_fr ,
    msg_2_0_fr , msg_2_1_fr , msg_2_2_fr , msg_2_3_fr , msg_2_4_fr , msg_2_5_fr , msg_2_6_fr , msg_2_7_fr ,
    msg_3_0_fr , msg_3_1_fr , msg_3_2    , msg_3_3_fr , msg_3_4    , msg_3_5    , msg_3_6    , msg_3_7_fr ,
    msg_4_0    , msg_4_1_fr , msg_4_2    , msg_4_3    , msg_4_4_fr , msg_4_5_fr , msg_4_6_fr , msg_4_7_fr ,
    msg_5_0_fr , msg_5_1_fr , msg_5_2_fr , msg_5_3_fr , msg_5_4_fr , msg_5_5_fr , msg_5_6_fr , msg_5_7    ,
    msg_6_0_fr , msg_6_1    , msg_6_2_fr , msg_6_3_fr , msg_6_4    , msg_6_5_fr , msg_6_6_fr , msg_6_7_fr ,
    msg_7_0_fr , msg_7_1_fr , msg_7_2_fr , msg_7_3_fr , msg_7_4    , msg_7_5_fr , msg_7_6_fr , msg_7_7_fr ,
    msg_8_0_fr , msg_8_1_fr , msg_8_2_fr , msg_8_3_fr , msg_8_4_fr , msg_8_5_fr , msg_8_6_fr , msg_8_7_fr ,
    0, 0, 0, 0, 0, 0, 0, 0,
    msg_10_0, msg_10_1, msg_10_2, msg_10_3, msg_10_4, msg_10_5, msg_10_6, msg_10_7,
    msg_11_0, msg_11_1, msg_11_2, msg_11_3, msg_11_4, msg_11_5, msg_11_6, msg_11_7,
    msg_12_0, msg_12_1, msg_12_2, msg_12_3, msg_12_4, msg_12_5, msg_12_6, msg_12_7,
    msg_13_0, msg_13_1, msg_13_2, msg_13_3, msg_13_4, msg_13_5, msg_13_6, msg_13_7,
    msg_14_0, msg_14_1, msg_14_2, msg_14_3, msg_14_4, msg_14_5, msg_14_6, msg_14_7,
    msg_15_0, msg_15_1, msg_15_2, msg_15_3, msg_15_4, msg_15_5, msg_15_6, msg_15_7
};

const char *const msgTable_ger[] PROGMEM =
{
    msg_0_0_ger, msg_0_1    , msg_0_2_ger, msg_0_3_ger, msg_0_4_ger, msg_0_5_ger, msg_0_6_ger, msg_0_7_ger,
    msg_1_0_ger, msg_1_1    , msg_1_2_ger, msg_1_3_ger, msg_1_4_ger, msg_1_5_ger, msg_1_6_ger, msg_1_7_ger,
    msg_2_0_ger, msg_2_1_ger, msg_2_2_ger, msg_2_3_ger, msg_2_4_ger, msg_2_5_ger, msg_2_6_ger, msg_2_7_ger,
    msg_3_0_ger, msg_3_1_ger, msg_3_2    , msg_3_3_ger, msg_3_4    , msg_3_5    , msg_3_6    , msg_3_7_ger,
    msg_4_0    , msg_4_1_ger, msg_4_2    , msg_4_3    , msg_4_4_ger, msg_4_5_ger, msg_4_6_ger, msg_4_7_ger,
    msg_5_0_ger, msg_5_1_ger, msg_5_2_ger, msg_5_3_ger, msg_5_4_ger, msg_5_5_ger, msg_5_6_ger, msg_5_7    ,
    msg_6_0_ger, msg_6_1    , msg_6_2_ger, msg_6_3_ger, msg_6_4    , msg_6_5_ger, msg_6_6_ger, msg_6_7_ger,
    msg_7_0_ger, msg_7_1_ger, msg_7_2_ger, msg_7_3_ger, msg_7_4    , msg_7_5_ger, msg_7_6_ger, msg_7_7_ger,
    msg_8_0_ger, msg_8_1_ger, msg_8_2_ger, msg_8_3_ger, msg_8_4_ger, msg_8_5_ger, msg_8_6_ger, msg_8_7_ger,
    0, 0, 0, 0, 0, 0, 0, 0,
    msg_10_0, msg_10_1, msg_10_2, msg_10_3, msg_10_4, msg_10_5, msg_10_6, msg_10_7,
    msg_11_0, msg_11_1, msg_11_2, msg_11_3, msg_11_4, msg_11_5, msg_11_6, msg_11_7,
    msg_12_0, msg_12_1, msg_12_2, msg_12_3, msg_12_4, msg_12_5, msg_12_6, msg_12_7,
    msg_13_0, msg_13_1, msg_13_2, msg_13_3, msg_13_4, msg_13_5, msg_13_6, msg_13_7,
    msg_14_0, msg_14_1, msg_14_2, msg_14_3, msg_14_4, msg_14_5, msg_14_6, msg_14_7,
    msg_15_0, msg_15_1, msg_15_2, msg_15_3, msg_15_4, msg_15_5, msg_15_6, msg_15_7
};

const char *const msgTable_spa[] PROGMEM =
{
    msg_0_0_spa, msg_0_1    , msg_0_2_spa, msg_0_3_spa, msg_0_4_spa, msg_0_5_spa, msg_0_6_spa, msg_0_7_spa,
    msg_1_0_spa, msg_1_1    , msg_1_2_spa, msg_1_3_spa, msg_1_4_spa, msg_1_5_spa, msg_1_6_spa, msg_1_7_spa,
    msg_2_0_spa, msg_2_1_spa, msg_2_2_spa, msg_2_3_spa, msg_2_4_spa, msg_2_5_spa, msg_2_6_spa, msg_2_7_spa,
    msg_3_0_spa, msg_3_1_spa, msg_3_2    , msg_3_3_spa, msg_3_4    , msg_3_5    , msg_3_6    , msg_3_7_spa,
    msg_4_0    , msg_4_1_spa, msg_4_2    , msg_4_3    , msg_4_4_spa, msg_4_5_spa, msg_4_6_spa, msg_4_7_spa,
    msg_5_0_spa, msg_5_1_spa, msg_5_2_spa, msg_5_3_spa, msg_5_4_spa, msg_5_5_spa, msg_5_6_spa, msg_5_7    ,
    msg_6_0_spa, msg_6_1    , msg_6_2_spa, msg_6_3_spa, msg_6_4    , msg_6_5_spa, msg_6_6_spa, msg_6_7_spa,
    msg_7_0_spa, msg_7_1_spa, msg_7_2_spa, msg_7_3_spa, msg_7_4    , msg_7_5_spa, msg_7_6_spa, msg_7_7_spa,
    msg_8_0_spa, msg_8_1_spa, msg_8_2_spa, msg_8_3_spa, msg_8_4_spa, msg_8_5_spa, msg_8_6_spa, msg_8_7_spa,
    0, 0, 0, 0, 0, 0, 0, 0,
    msg_10_0, msg_10_1, msg_10_2, msg_10_3, msg_10_4, msg_10_5, msg_10_6, msg_10_7,
    msg_11_0, msg_11_1, msg_11_2, msg_11_3, msg_11_4, msg_11_5, msg_11_6, msg_11_7,
    msg_12_0, msg_12_1, msg_12_2, msg_12_3, msg_12_4, msg_12_5, msg_12_6, msg_12_7,
    msg_13_0, msg_13_1, msg_13_2, msg_13_3, msg_13_4, msg_13_5, msg_13_6, msg_13_7,
    msg_14_0, msg_14_1, msg_14_2, msg_14_3, msg_14_4, msg_14_5, msg_14_6, msg_14_7,
    msg_15_0, msg_15_1, msg_15_2, msg_15_3, msg_15_4, msg_15_5, msg_15_6, msg_15_7
};

const char *const msgTable_ita[] PROGMEM =
{
    msg_0_0_ita, msg_0_1    , msg_0_2_ita, msg_0_3_ita, msg_0_4_ita, msg_0_5_ita, msg_0_6_ita, msg_0_7_ita,
    msg_1_0_ita, msg_1_1    , msg_1_2_ita, msg_1_3_ita, msg_1_4_ita, msg_1_5_ita, msg_1_6_ita, msg_1_7_ita,
    msg_2_0_ita, msg_2_1_ita, msg_2_2_ita, msg_2_3_ita, msg_2_4_ita, msg_2_5_ita, msg_2_6_ita, msg_2_7_ita,
    msg_3_0_ita, msg_3_1_ita, msg_3_2    , msg_3_3_ita, msg_3_4    , msg_3_5    , msg_3_6    , msg_3_7_ita,
    msg_4_0    , msg_4_1_ita, msg_4_2    , msg_4_3    , msg_4_4_ita, msg_4_5_ita, msg_4_6_ita, msg_4_7_ita,
    msg_5_0_ita, msg_5_1_ita, msg_5_2_ita, msg_5_3_ita, msg_5_4_ita, msg_5_5_ita, msg_5_6_ita, msg_5_7    ,
    msg_6_0_ita, msg_6_1    , msg_6_2_ita, msg_6_3_ita, msg_6_4    , msg_6_5_ita, msg_6_6_ita, msg_6_7_ita,
    msg_7_0_ita, msg_7_1_ita, msg_7_2_ita, msg_7_3_ita, msg_7_4    , msg_7_5_ita, msg_7_6_ita, msg_7_7_ita,
    msg_8_0_ita, msg_8_1_ita, msg_8_2_ita, msg_8_3_ita, msg_8_4_ita, msg_8_5_ita, msg_8_6_ita, msg_8_7_ita,
    0, 0, 0, 0, 0, 0, 0, 0,
    msg_10_0, msg_10_1, msg_10_2, msg_10_3, msg_10_4, msg_10_5, msg_10_6, msg_10_7,
    msg_11_0, msg_11_1, msg_11_2, msg_11_3, msg_11_4, msg_11_5, msg_11_6, msg_11_7,
    msg_12_0, msg_12_1, msg_12_2, msg_12_3, msg_12_4, msg_12_5, msg_12_6, msg_12_7,
    msg_13_0, msg_13_1, msg_13_2, msg_13_3, msg_13_4, msg_13_5, msg_13_6, msg_13_7,
    msg_14_0, msg_14_1, msg_14_2, msg_14_3, msg_14_4, msg_14_5, msg_14_6, msg_14_7,
    msg_15_0, msg_15_1, msg_15_2, msg_15_3, msg_15_4, msg_15_5, msg_15_6, msg_15_7
};

const char *const msgTable_dut[] PROGMEM =
{
    msg_0_0_dut, msg_0_1    , msg_0_2_dut, msg_0_3_dut, msg_0_4_dut, msg_0_5_dut, msg_0_6_dut, msg_0_7_dut,
    msg_1_0_dut, msg_1_1    , msg_1_2_dut, msg_1_3_dut, msg_1_4_dut, msg_1_5_dut, msg_1_6_dut, msg_1_7_dut,
    msg_2_0_dut, msg_2_1_dut, msg_2_2_dut, msg_2_3_dut, msg_2_4_dut, msg_2_5_dut, msg_2_6_dut, msg_2_7_dut,
    msg_3_0_dut, msg_3_1_dut, msg_3_2    , msg_3_3_dut, msg_3_4    , msg_3_5    , msg_3_6    , msg_3_7_dut,
    msg_4_0    , msg_4_1_dut, msg_4_2    , msg_4_3    , msg_4_4_dut, msg_4_5_dut, msg_4_6_dut, msg_4_7_dut,
    msg_5_0_dut, msg_5_1_dut, msg_5_2_dut, msg_5_3_dut, msg_5_4_dut, msg_5_5_dut, msg_5_6_dut, msg_5_7    ,
    msg_6_0_dut, msg_6_1    , msg_6_2_dut, msg_6_3_dut, msg_6_4    , msg_6_5_dut, msg_6_6_dut, msg_6_7_dut,
    msg_7_0_dut, msg_7_1_dut, msg_7_2_dut, msg_7_3_dut, msg_7_4    , msg_7_5_dut, msg_7_6_dut, msg_7_7_dut,
    msg_8_0_dut, msg_8_1_dut, msg_8_2_dut, msg_8_3_dut, msg_8_4_dut, msg_8_5_dut, msg_8_6_dut, msg_8_7_dut,
    0, 0, 0, 0, 0, 0, 0, 0,
    msg_10_0, msg_10_1, msg_10_2, msg_10_3, msg_10_4, msg_10_5, msg_10_6, msg_10_7,
    msg_11_0, msg_11_1, msg_11_2, msg_11_3, msg_11_4, msg_11_5, msg_11_6, msg_11_7,
    msg_12_0, msg_12_1, msg_12_2, msg_12_3, msg_12_4, msg_12_5, msg_12_6, msg_12_7,
    msg_13_0, msg_13_1, msg_13_2, msg_13_3, msg_13_4, msg_13_5, msg_13_6, msg_13_7,
    msg_14_0, msg_14_1, msg_14_2, msg_14_3, msg_14_4, msg_14_5, msg_14_6, msg_14_7,
    msg_15_0, msg_15_1, msg_15_2, msg_15_3, msg_15_4, msg_15_5, msg_15_6, msg_15_7
};
