# speedtest-c

![build](https://github.com/nojusdau/Speedtest_project/actions/workflows/build.yml/badge.svg)
![license](https://img.shields.io/badge/license-MIT-blue.svg)

Konsoline C programa interneto rysio parsiuntimo / issiuntimo greiciui
ismatuoti, panasiai kaip `speedtest-cli`. Programa naudoja **Speedtest
Mini (Ookla)** tipo serveriu sarasa (`speedtest_server_list.json`),
nustato vartotojo vietove per viesa IP-API paslauga, ir leidzia atlikti
kiekviena veiksma atskirai arba pilna automatizuota testa.

## Greitas startas

```bash
git clone https://github.com/nojusdau/Speedtest_project.git
cd <REPO>
./install.sh          # patikrina/idiegia priklausomybes ir paruosia programa darbui
./bin/speedtest --all  # paleidzia pilna automatizuota testa
```

## Naudojamos bibliotekos

| Biblioteka | Paskirtis                                                 |
|------------|-----------------------------------------------------------|
| `libcurl`  | HTTP uzklausos (parsisiuntimas, issiuntimas, vietoves API)|
| `getopt`   | Komandines eilutes parametru apdorojimas                  |
| `cJSON`    | JSON failo (serveriu sarasas) ir API atsakymu apdorojimas |
|            | (vendored `src/cJSON.c` / `src/cJSON.h`,                  |
|            | is https://github.com/DaveGamble/cJSON)                   |

## Kompiliavimas rankiniu budu

Jei nenorite naudoti `install.sh` (pvz. priklausomybes jau idiegtos):

```bash
make
```

Sukompiliuota programa atsiranda `bin/speedtest`.

```bash
make clean   # isvalo .o failus ir bin/ katalogu
```

## Serveriu sarasas

Repozitorijoje esantis `speedtest_server_list.json` yra serveriu sarasas, 
skirtas programos veikimui pademonstruoti.
Naujas pateiktas failas turi buti patalpintas projekto
sakninameme kataloge tuo paciu pavadinimu (arba jo kelias nurodytas per
`--json-file`).

Kiekvienas irasas faile turi lauka: `country`, `city`, `provider`,
`host` (formatu `host:portas`) ir `id`.

## Naudojimas

```text
Naudojimas: ./bin/speedtest [PASIRINKTYS]

Veiksmai (galima rinktis viena ar keleta; jei neduota nei vieno,
rodoma pagalba):
  -a, --all                 Atlikti visa automatizuota testa
                             (vietove -> geriausias serveris ->
                              parsiuntimas -> issiuntimas)
  -l, --location             Nustatyti tik vartotojo vietove (valstybe)
  -f, --find-server           Rasti geriausia serveri pagal vietove
  -d, --download              Atlikti parsisiuntimo greicio testa
  -u, --upload                 Atlikti issiuntimo greicio testa

Parametrai:
  -s, --server HOST:PORT     Serveris, naudojamas -d / -u testams,
                              jei nenaudojamas -f ar -a
  -c, --country VALSTYBE     Valstybes pavadinimas, naudojamas su -f
  -j, --json-file KELIAS     Kelias iki serveriu saraso JSON failo
                              (numatytasis: speedtest_server_list.json)
  -t, --timeout SEKUNDES     Testo trukme sekundemis (maksimalu 15)
  -h, --help                  Rodyti pagalba
```

### Pavyzdziai

Pilnas automatizuotas testas (vietove -> serveris -> parsiuntimas ->
issiuntimas):

```bash
./bin/speedtest --all
```

Tik vietoves nustatymas:

```bash
./bin/speedtest --location
```

Geriausio serverio paieska pagal nurodyta valstybe (be automatinio
vietoves nustatymo):

```bash
./bin/speedtest --find-server --country Lithuania
```

Parsisiuntimo testas su konkreciu serveriu (individualus veiksmas -
reikalingas `--server` parametras):

```bash
./bin/speedtest --download --server speedtest.example.com:8080
```

Issiuntimo testas su trumpesne trukme:

```bash
./bin/speedtest --upload --server speedtest.example.com:8080 --timeout 8
```

Keleto veiksmu derinimas (pvz. rasti serveri ir issitesti abu greicio
testus be pilno `--all`):

```bash
./bin/speedtest --find-server --download --upload --country Lithuania
```

## Kaip veikia matavimas

* **Parsisiuntimas** - is serverio pakartotinai (kol nesueina laiko
  limitas) siunciamas `speedtest/random<N>x<N>.jpg` atsitiktinio dydzio
  paveiksliukas, matuojant realiai gauta baitu kieki per praejusi laika.
* **Issiuntimas** - i serveri (`speedtest/upload.php`) srautiniu budu
  (`Transfer-Encoding: chunked`) siunciami atsitiktiniai duomenys, kol
  sueina laiko limitas.
* Abu testai automatiskai nutraukiami po **daugiausiai 15 sekundziu**
  (naudojant `libcurl` progreso griztamaji rysi).
* Rezultatas visada pateikiamas **megabitais per sekunde (Mbps)**.
* **Vietove** nustatoma per viesa, nemokama API `http://ip-api.com/json/`.
* **Geriausias serveris** parenkamas pagal maziausia atsako laika
  (latency) is kandidatu, kuriu `country` laukas atitinka nustatyta /
  nurodyta valstybe (tikrinama iki `CANDIDATE_LIMIT` = 15 serveriu).

## Klaidu valdymas

Programa korektiskai apdoroja ir prasmingai praneša apie:

* trukstamus / neteisingus komandines eilutes parametrus;
* neegzistuojanti ar netinkamo formato serveriu saraso JSON faila;
* nepasiekiama vietoves API arba netinkama API atsakyma;
* neaktyvius / nepasiekiamus speedtest serverius (tiek ieskant
  geriausio serverio, tiek atliekant parsisiuntimo/issiuntimo testus) -
  tokiu atveju programa nenutrūksta avariškai, o grąžina klaidos
  pranešimą ir tęsia likusius užduotus veiksmus.

Klaidos praneszamos i `stderr`, o programa baigia darba su ne-nuliniu
isejimo kodu (`exit code 1`), jei bent vienas veiksmas nepavyko.

## Projekto struktura

```
speedtest_project/
├── Makefile
├── README.md
├── speedtest_server_list.json
└── src/
    ├── main.c           - CLI apdorojimas ir veiksmu orkestravimas
    ├── common.h         - bendros struktūros ir konstantos
    ├── http_utils.c/h   - libcurl pagalbines funkcijos (write/progress callback)
    ├── location.c/h     - vietoves nustatymas per IP-API
    ├── servers.c/h      - serveriu saraso ikelimas, filtravimas, latency testas
    ├── speedtest.c/h    - parsisiuntimo / issiuntimo greicio matavimas
    ├── cJSON.c/h        - vendored cJSON biblioteka
```
