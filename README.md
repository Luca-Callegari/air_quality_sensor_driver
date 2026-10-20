# Air Quality Sensor Driver

Documentazione d'uso e analisi tecnica del pacchetto ROS 2 `server_raw`, responsabile della lettura via porta seriale dei dati grezzi del sensore di qualità dell'aria (`Arduino Mega 2560`), della loro pubblicazione su topic ROS2 e della loro conversione in grandezze fisiche.

Il launch file vive in un pacchetto separato, `ros2_bringup`.

## Indice

- [Air Quality Sensor Driver](#air-quality-sensor-driver)
  - [Indice](#indice)
  - [1. Avvio del sistema](#1-avvio-del-sistema)
    - [1.1 Collegare il sensore](#11-collegare-il-sensore)
    - [1.2 Verificare che la porta USB sia rilevata dal container](#12-verificare-che-la-porta-usb-sia-rilevata-dal-container)
    - [1.3 Posizionarsi nella root del workspace](#13-posizionarsi-nella-root-del-workspace)
    - [1.4 Build del workspace](#14-build-del-workspace)
    - [1.5 Sourcing dell'ambiente](#15-sourcing-dellambiente)
    - [1.6 Lanciare il launch file](#16-lanciare-il-launch-file)
  - [2. Verifica del funzionamento](#2-verifica-del-funzionamento)
    - [2.1 Aprire un secondo terminale nel container](#21-aprire-un-secondo-terminale-nel-container)
    - [2.2 Verificare i topic attivi](#22-verificare-i-topic-attivi)
    - [2.3 Leggere i dati pubblicati](#23-leggere-i-dati-pubblicati)
    - [2.4 Abilitare/disabilitare il nodo tramite servizio](#24-abilitaredisabilitare-il-nodo-tramite-servizio)
    - [2.5 Gestione dello scollegamento della porta seriale](#25-gestione-dello-scollegamento-della-porta-seriale)
  - [3. Analisi file `CMakeLists.txt`](#3-analisi-file-cmakeliststxt)
    - [3.1 Server component](#31-server-component)
    - [3.2 Gestione eseguibile](#32-gestione-eseguibile)
  - [4. Analisi file `package.xml`](#4-analisi-file-packagexml)

---

## 1. Avvio del sistema

### 1.1 Collegare il sensore
Collegare il sensore alla porta USB del computer host (o del container).

### 1.2 Verificare che la porta USB sia rilevata dal container

```bash
ls -l /dev/ttyUSB*
```

Deve comparire il device (es. `/dev/ttyUSB0`), coerente con il parametro `sensor.device.file` configurato in `params.yaml`.

### 1.3 Posizionarsi nella root del workspace

```bash
cd ~/workspace 
```

### 1.4 Build del workspace

```bash
colcon build
```

Questo comando compila sia `server_raw` sia `ros2_bringup` (e le eventuali altre dipendenze del workspace).

### 1.5 Sourcing dell'ambiente

```bash
source install/setup.zsh
```

> Sostituire `setup.zsh` con `setup.bash` se si utilizza Bash invece di Zsh.

### 1.6 Lanciare il launch file

```bash
ros2 launch ros2_bringup air_quality.launch.py
```

Il launch file istanzia un `ComposableNodeContainer` (`component_container_mt`, multi-thread) e vi carica come componente il nodo `server_components::ReaderNode` del pacchetto `server_raw`.

**NOTA**: per come è stato configurato il CMakeLists.txt, è possibile creare un eseguibile `main_cpp` che permetta di gestire tutte le funzionalità del pacchetto `server_raw`, esattamente come il launch file.
Eseguire pertanto il comando:

```bash
ros2 run server_raw main_cpp
```

---

## 2. Verifica del funzionamento

### 2.1 Aprire un secondo terminale nel container

Con il container già in esecuzione, aprire una seconda shell al suo interno.

### 2.2 Verificare i topic attivi

```bash
ros2 topic list
```

Dovrebbero comparire, tra gli altri:
- `/server_components/readings_raw`
- `/server_components/readings`

(il prefisso dipende dal namespace `server_components` impostato nel launch file).

### 2.3 Leggere i dati pubblicati

Una volta verificato che `readings` e `readings_raw` sono attivi, sceglierne uno:

```bash
ros2 topic echo <nome_topic>
```

- **`readings_raw`**: contiene la stringa JSON così come arriva dal sensore via seriale, senza alcuna elaborazione.
- **`readings`**: contiene i dati post-processati (conversioni di unità di misura) in formato JSON.
Per il post-processing dei dati sono state applicate le conversioni presenti e descritte nel seguente link github: https://gist.github.com/robmasocco/bfe862bd41fecfa7268ba413bd229e73

### 2.4 Abilitare/disabilitare il nodo tramite servizio

Il servizio `/srv_set_enable` permette di abilitare o disabilitare la lettura/pubblicazione a runtime:

```bash
ros2 service call /srv_set_enable std_srvs/srv/SetBool "{data: true/false}"
```

- Il parametro `autostart` (presente nel file `yaml`) è di default `true`, e alla costruzione del nodo viene impostato `is_running_ = autostart_`: il campionamento parte quindi automaticamente all'avvio.
- Chiamando il servizio con `data: false` si imposta `is_running_ = false`, la porta seriale viene chiusa e il timer viene cancellato: la pubblicazione sui due topic si interrompe.
- Chiamando il servizio con `data: true` si imposta `is_running_ = true` e, se il timer era stato cancellato, viene riattivato (`serial_timer_->reset()`), riprendendo la lettura/pubblicazione.

### 2.5 Gestione dello scollegamento della porta seriale

Lo scollegamento fisico della porta seriale durante il funzionamento è gestito in modo robusto dal nodo:
- se la porta non è aperta, il callback tenta di riaprirla ad ogni tick del timer;
- se durante la lettura si verifica una `serial::IOException`, la porta viene chiusa in modo sicuro e il nodo ritenterà l'apertura al ciclo successivo;
- non è quindi necessario riavviare manualmente il nodo in caso di disconnessione/riconnessione del sensore.

---

## 3. Analisi file `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.8)
project(server_raw)
```
Versione minima di CMake richiesta e nome del progetto/pacchetto (`server_raw`), che deve coincidere con quello dichiarato in `package.xml`.

```cmake
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
```
Se il compilatore è GCC o Clang, abilita flag di warning.

```cmake
find_package(ament_cmake REQUIRED)
```
Dipendenza obbligatoria per qualunque pacchetto `ament_cmake`: fornisce le macro ROS2 (`ament_package()`, `ament_target_dependencies()`, ecc.).

```cmake
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(rclcpp_components REQUIRED)
```
Dipendenze ROS2 **ament-aware**: client library C++ (`rclcpp`), messaggi standard (`std_msgs`) e framework per nodi componibili (`rclcpp_components`). Sono tutte trovate tramite i rispettivi file `*Config.cmake` esportati dai pacchetti ROS2, e sono dichiarate anche in `package.xml` come `<depend>`.

```cmake
find_package(nlohmann_json REQUIRED)
```
 **Libreria non ament-aware.** `nlohmann_json` (https://github.com/nlohmann/json) non è un pacchetto ROS2: non ha un `package.xml` né viene risolta dall'indice di pacchetti `ament`. Il `find_package()` funziona comunque perché la libreria, una volta installata a livello di sistema, esporta un file di configurazione CMake standard (`nlohmann_jsonConfig.cmake`) nel path di sistema (`/usr/lib/cmake/...` o simile), che CMake individua "in modo nativo" indipendentemente da `ament`. Per questo motivo **non compare** tra i `<depend>` di `package.xml`: `rosdep`/`ament` non la conoscono e non la gestiscono, va installata a parte come dipendenza di sistema.

```cmake
find_package(params_manager_cpp REQUIRED)
```
 **Libreria non ament-aware.** Stesso discorso di `nlohmann_json`: `params_manager_cpp` (https://github.com/dotX-Automation/params_manager_cpp) è una libreria esterna a dotX-Automation, non un pacchetto `ament_cmake`. Va installata manualmente sul sistema (o nel container) prima della build; il suo file di config CMake (`params_manager_cppConfig.cmake`) viene individuato da CMake fuori dal meccanismo di scoperta di `ament_index`. Anche questa **non compare** in `package.xml`.

```cmake
find_package(std_srvs REQUIRED)
```
Dipendenza ROS2 ament-aware che fornisce il tipo di servizio `std_srvs/srv/SetBool` usato da `/srv_set_enable` e presente in `package.xml`.

```cmake
find_library(SERIAL_LIB serial REQUIRED)
find_path(SERIAL_INCLUDE_DIR serial/serial.h REQUIRED)
```
**Libreria non ament-aware (e senza CMake config package).** A differenza delle due precedenti, la libreria `serial` (https://github.com/dotX-Automation/serial) non fornisce un file `*Config.cmake`: per questo qui si usano i comandi CMake "a basso livello" `find_library()` (cerca il file binario) e `find_path()` (cerca l'header `serial/serial.h`), che effettuano una ricerca nei path standard di sistema. Il flag `REQUIRED` fa fallire la configurazione se libreria o header non vengono trovati. Anche questa dipendenza **non compare** in `package.xml`, poiché non è un pacchetto ROS/ament e va installata manualmente sul sistema prima della build.

```cmake
include_directories(
  include
  ${SERIAL_INCLUDE_DIR}
)
```
Aggiunge la cartella `include` locale del pacchetto e la cartella header trovata per `serial` al path di ricerca globale degli header (si applica a tutti i target dichiarati dopo questa riga).

```cmake
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/src")
```
Crea a runtime di configurazione la cartella `build/.../src`, necessaria per ospitare il file sorgente generato al passo successivo.

```cmake
generate_init_parameters(
  YAML_FILE "config/params.yaml"
  OUT_FILE "src/init_parameters.cpp")
```
Macro CMake **fornita da `params_manager_cpp`**: legge `config/params.yaml` e genera automaticamente, in fase di build, il file `src/init_parameters.cpp` contenente il codice C++ di dichiarazione/inizializzazione dei parametri ROS2 (nomi, tipi, default, range, flag `read_only`, ecc.) descritti nello YAML. Questo evita di scrivere a mano tutte le dichiarazioni per ogni parametro.

### 3.1 Server component

```cmake
add_library(server SHARED
  src/server.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/src/init_parameters.cpp
)
```
Definisce la libreria condivisa `server`, che contiene sia il codice del nodo (`server.cpp`) sia il codice generato per i parametri. È una libreria condivisa (`SHARED`) perché deve poter essere caricata dinamicamente da `rclcpp_components` come componente.

Non è presente `main.cpp` perché questo è un semplice eseguibile e non contiene tutta la logica del nodo (ReaderNode, lettura seriale, callback, ecc.).

```cmake
target_compile_definitions(server PRIVATE COMPOSITION_BUILDING_DLL)
target_include_directories(server PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${SERIAL_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/include
  )
```
Espone come `PUBLIC` gli header path: quello locale in fase di build, quello installato in fase di uso post-installazione (pattern standard `BUILD_INTERFACE`/`INSTALL_INTERFACE`), l'include path di `serial`, e un path generato in `build/.../include`.


```cmake
ament_target_dependencies(server
  rclcpp
  rclcpp_components
  std_msgs
  std_srvs
  params_manager_cpp
  )
```
Collega automaticamente i flag di compilazione delle dipendenze **ament-aware**. Qui compare anche `params_manager_cpp`: sebbene non sia un pacchetto ROS/ament, se esporta comunque le variabili nello stile ament-compatibile (o un target CMake standard) `ament_target_dependencies()` è in grado di riconoscerlo correttamente, oppure la macro è tollerante verso pacchetti trovati come normali pacchetti CMake. In pratica ciò che conta è che `find_package(params_manager_cpp REQUIRED)` sia andato a buon fine sopra.

```cmake
target_link_libraries(server
  ${SERIAL_LIB}
  nlohmann_json::nlohmann_json
  params_manager_cpp::paramsmanager
  )
```
Linking esplicito delle librerie **non ament-aware**:
- `${SERIAL_LIB}`: il path trovato da `find_library()` per `serial`.
- `nlohmann_json::nlohmann_json`: il target CMake `IMPORTED` esportato dal config package di `nlohmann_json`.
- `params_manager_cpp::paramsmanager`: il target `IMPORTED` esportato dal config package di `params_manager_cpp`.

Per poter agganciare fra di loro le librerie esterne viene chiamato il seguente comando:

```cmake
rclcpp_components_register_nodes(server "server_components::ReaderNode")
```
Macro di `rclcpp_components` che registra la classe `server_components::ReaderNode` come nodo componibile, rendendola disponibile per essere istanziata via `ComposableNode` nei container (come avviene nel launch file di `ros2_bringup`) o via `ros2 component load`.

### 3.2 Gestione eseguibile

Il target `main_cpp` è stato utilizzato in fase di test, prima dell'introduzione del launch file, come metodo alternativo per eseguire il nodo `ReaderNode` da riga di comando, senza dover passare da un container di componenti.

Con l'introduzione del launch file in `ros2_bringup`, questo target non è più necessario al funzionamento del sistema: il launch file istanzia infatti un `component_container_mt` dedicato, che carica  `ReaderNode` come componente a partire dalla libreria condivisa `server` (generata da `server.cpp`), indipendentemente dalla presenza dell'eseguibile `main_cpp`.

Di conseguenza, la sezione del `CMakeLists.txt` — relativa alla definizione, configurazione e installazione del target `main_cpp` — può essere rimossa senza compromettere il funzionamento del nodo tramite `ros2 launch ros2_bringup air_quality.launch.py`.

L'unico effetto della rimozione riguarda la possibilità di eseguire il nodo come processo: il comando

```bash
ros2 run server_raw main_cpp
```

non sarebbe più disponibile, poiché l'eseguibile `main_cpp` non verrebbe più compilato né installato.

```cmake
add_executable(main_cpp 
  src/main.cpp 
)
```
Definisce l'eseguibile `main_cpp`, alternativa "non componibile" per eseguire il nodo direttamente (senza container).

```cmake
target_compile_features(main_cpp PUBLIC c_std_99 cxx_std_17)
```
Imposta lo standard C99/C++17 richiesto per compilare `main_cpp`.

```cmake
target_include_directories(main_cpp PRIVATE
  ${SERIAL_INCLUDE_DIR}
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  )
```
Include path privati per `main_cpp`.

```cmake
ament_target_dependencies(main_cpp
  rclcpp
  rclcpp_components
  std_msgs
  std_srvs
  )
```
Dipendenze ament-aware per l'eseguibile. Da notare che qui non compaiono `nlohmann_json`, `params_manager_cpp` né `serial`: `main_cpp` le eredita tramite il linking con `server` (proprietà `PUBLIC` viste sopra).

```cmake
target_link_libraries(main_cpp
  server
  )
```
`main_cpp` linka la libreria `server`: qui avviene il collegamento con tutto ciò che `server` espone pubblicamente, incluse le librerie non ament-aware sopra citate.

```cmake
install(TARGETS server
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
```
Regole di installazione standard per la libreria condivisa `server`, necessarie perché sia poi individuabile a runtime dal component container.

```cmake
install(TARGETS main_cpp
  DESTINATION lib/${PROJECT_NAME})
```
Installa l'eseguibile `main_cpp` nel path convenzionale ROS 2 (`lib/server_raw/main_cpp`), da cui `ros2 run server_raw main_cpp` potrebbe lanciarlo.

```cmake
install(DIRECTORY config include/
  DESTINATION share/${PROJECT_NAME}
)
```
Installa la cartella `config` (contenente `params.yaml`) e il contenuto di `include/` sotto `share/server_raw`, rendendoli disponibili anche dopo l'installazione (es. per essere referenziati da `ros2_bringup`).

```cmake
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
```


---

## 4. Analisi file `package.xml`


```xml
<depend>rclcpp</depend>
<depend>std_msgs</depend>
<depend>std_srvs</depend>
<depend>rclcpp_components</depend>
```
Dipendenze ROS2 **ament-aware**, usate sia in fase di build sia di run.

> **Nota:** qui non compaiono `nlohmann_json`, `params_manager_cpp` né `serial`, cioè le tre librerie esterne non ament-aware usate nel CMakeLists, poiché il meccanismo `<depend>` di `package.xml` serve a `rosdep`/`colcon` per risolvere dipendenze *tra pacchetti ROS/ament*. Librerie esterne installate via un proprio processo di build/install, non tramite un pacchetto `ament`, non hanno un identificatore `rosdep`/`ament`. Di conseguenza:
> - `colcon build` non saprà mai automaticamente che il pacchetto dipende da queste librerie;
> - vanno installate manualmente (o via setup/Dockerfile) prima di lanciare `colcon build`, altrimenti i `find_package`/`find_library` REQUIRED nel CMake falliranno;

