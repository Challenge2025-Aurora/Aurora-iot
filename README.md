# Aurora-IoT — README

Sistema simples de telemetria para **monitorar vagas de motos**. Detecta presença, identifica o **slot** configurado pelo operador e publica dados em nuvem (**ThingSpeak**) para visualização e histórico em tempo real. Compatível com **Wokwi (simulação)** e **hardware real**.

---

## Sumário

* [Como funciona](#como-funciona)
* [Recursos do dispositivo](#recursos-do-dispositivo)
* [Regras de detecção](#regras-de-detecção)
* [Fluxo de dados (ThingSpeak)](#fluxo-de-dados-thingspeak)
* [Hardware e ligação (ESP32)](#hardware-e-ligação-esp32)
* [Simulação no Wokwi](#simulação-no-wokwi)
* [Configuração do ThingSpeak](#configuração-do-thingspeak)
* [Configuração do firmware](#configuração-do-firmware)
* [Interface serial e comandos](#interface-serial-e-comandos)
* [Operação típica](#operação-típica)
* [Dashboards no ThingSpeak](#dashboards-no-thingspeak)
* [Cenários de teste](#cenários-de-teste)
* [Checklist Sprint 3 (IoT)](#checklist-sprint-3-iot)

---

## Como funciona

1. O **ESP32** conecta-se ao Wi‑Fi e, a cada ciclo, **lê os sensores**:

   * **HC-SR04** (ultrassom) → distância até a moto (detecção de **presença**).
   * **Potenciômetro** → simula **bateria %**.
   * **Botões** → definem **slot** (área A/B, incremento numérico, limpar).
2. O firmware aplica as **regras** (ocupado/livre, fora do lugar, ocupação desconhecida, bateria baixa).
3. A cada ~**16 s** (limite do plano gratuito do ThingSpeak), **publica** os campos no canal.
4. **LEDs** e **buzzer** sinalizam estados e comandos (localizador).

---

## Recursos do dispositivo

**Microcontrolador**

* **ESP32 DevKit V1** – Wi‑Fi, publica os dados direto na internet (HTTP) para o ThingSpeak.

**Sensores**

* **HC-SR04 (ultrassom)** – mede `dist_cm` até a moto.
* **Potenciômetro** – simula `bateria_pct` (0–100%).

**Botões**

* `slot++` – incrementa o número do slot (**01…99**).
* `área A/B` – alterna letra da área (**A** ↔ **B**).
* `clear` – apaga a identificação (fica **sem slot**).

**LEDs**

* **Amarelo** – localizador (acende durante o **beep**).
* **Verde** – **vaga ocupada**.
* **Vermelho** – **fora do lugar** (*slot ≠ esperado*).
* **Azul** – **ocupação desconhecida** (*presença sem slot configurado*).
* **Laranja** – **bateria baixa** (*< 25%*).

**Atuador**

* **Buzzer** – toca quando recebe comando de **beep**.

---

## Regras de detecção

* **Presença**: `dist_cm < 20` → **ocupado**; caso contrário, **livre**.
* **Fora do lugar**: há **presença** e **há slot configurado** e `slot ≠ slot_esperado`.
* **Ocupação desconhecida**: há **presença** e **nenhum slot** configurado.
* **Bateria baixa**: `bateria_pct < 25`.

> Os limiares (`20 cm` e `25%`) são configuráveis no firmware.

---

## Fluxo de dados (ThingSpeak)

**Intervalo**: ~**16 s** (mínimo aceito no plano gratuito). A cada envio, o ESP32 publica:

| Campo    | Conteúdo                      | Exemplo           |
| -------- | ----------------------------- | ----------------- |
| `field1` | `dist_cm`                     | `18.7`            |
| `field2` | `presenca` (0/1)              | `1`               |
| `field3` | `slot` (texto)                | `"A-12"` ou `"-"` |
| `field4` | `bateria_pct` (0–100)         | `83`              |
| `field5` | `esperado` (texto)            | `"A-12"`          |
| `field6` | `fora_do_lugar` (0/1)         | `0`               |
| `field7` | `ocupacao_desconhecida` (0/1) | `0`               |

**Observação**: Campos **textuais** (`field3` e `field5`) ficam melhores como **Last Value** no painel. Os demais geram **gráficos**.

---

## Hardware e ligação (ESP32)

**Componentes**

* ESP32 DevKit V1
* HC-SR04 (ultrassom)
* Potenciômetro linear (10k)
* 5 LEDs (Amarelo, Verde, Vermelho, Azul, Laranja) + resistores (220–330Ω)
* 3 Botões (slot++, área A/B, clear) + resistores de pull-down (ou uso de pull‑ups internos)
* Buzzer passivo
* Jumpers e protoboard

**Pinagem sugerida** (ajuste conforme seu sketch):

| Sinal            | Componente                 | Pino ESP32 |
| ---------------- | -------------------------- | ---------- |
| TRIG             | HC-SR04                    | `GPIO26`   |
| ECHO             | HC-SR04                    | `GPIO25`   |
| POT              | Potenciômetro (ADC)        | `GPIO34`   |
| LED Amarelo      | Buzzer/locator (indicador) | `GPIO2`    |
| LED Verde        | Ocupado                    | `GPIO4`    |
| LED Vermelho     | Fora do lugar              | `GPIO16`   |
| LED Azul         | Ocupação desconhecida      | `GPIO17`   |
| LED Laranja      | Bateria baixa              | `GPIO5`    |
| Buzzer           | Bipe                       | `GPIO12`   |
| Botão `slot++`   | Incrementa slot            | `GPIO14`   |
| Botão `área A/B` | Alterna área               | `GPIO27`   |
| Botão `clear`    | Limpa slot                 | `GPIO33`   |

> **Dicas**:
>
> * Use resistores de 220–330Ω em série com cada LED.
> * Para botões, ative `pinMode(..., INPUT_PULLUP)` e trate **nível baixo** como pressionado.

---

## Simulação no Wokwi

1. Crie um projeto **ESP32**.
2. Adicione **HC-SR04**, **potenciômetro**, **buzzer**, **5 LEDs** e **3 botões**.
3. Conecte os pinos conforme a tabela acima.
4. Cole o **sketch** e ajuste **SSID**/**senha** (Wokwi usa `Wokwi-GUEST` sem senha).
5. Configure as **chaves do ThingSpeak** no código.
6. Clique em **Play** e observe no **Serial Monitor** os envios a cada ~16 s.

> No Wokwi, altere o **potenciômetro** para simular `bateria_pct`, e clique nos **botões** para mudar `slot` (`A-01…A-99`, `B-01…B-99`). Para presença, aproxime/afaste a leitura do **HC-SR04** (ajuste o parâmetro *distance* do sensor, se disponível).

---

## Configuração do ThingSpeak

1. Crie uma conta e um **Channel** chamado `Aurora-IoT`.
2. Ative os **Fields 1..7** na ordem:

   1. `dist_cm`
   2. `presenca`
   3. `slot`
   4. `bateria_pct`
   5. `esperado`
   6. `fora_do_lugar`
   7. `ocupacao_desconhecida`
3. Anote o **Channel ID** e a **Write API Key**.
4. Em **Sharing**, deixe **Private** (ou **Public** se quiser demonstrar) e salve.

---

## Configuração do firmware

No início do sketch, ajuste as **constantes**:

```cpp
// Wi-Fi
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// ThingSpeak
const char* TS_WRITE_API_KEY = "SUA_WRITE_KEY"; // obrigatória
const int   TS_CHANNEL_ID    = 123456;           // seu Channel ID
const unsigned long TS_MIN_INTERVAL_MS = 16000;  // ~16s

// Regras (limiares)
const float THRESH_PRESENCA_CM = 20.0; // < 20 cm = ocupado
const int   THRESH_BATERIA_PCT = 25;   // < 25% = baixa
```

> **Importante**: respeite **>= 15 s** entre envios (usamos **16 s**). Envios mais rápidos são ignorados pelo ThingSpeak no plano gratuito.

---

## Interface serial e comandos

Use o **Monitor Serial** (115200 bps) para consultar/ajustar:

```json
{"action":"beep","duration_ms":1500}
```

> Toca o **buzzer** e acende o **LED amarelo** pelo tempo indicado.

```json
{"set_expected":"A-12"}
```

> Define o **slot esperado** para comparação (`fora_do_lugar`).

```json
{"modo":"tabela"}
{"modo":"json"}
{"modo":"ambos"}
```

> Ajusta o **formato** de saída no terminal.

```json
{"status":true}
```

> Imprime um **snapshot** imediato do estado atual.

---

## Operação típica

1. O operador **define o slot** com os botões (**área** + **slot++**).
2. Ao estacionar a moto (**distância < 20 cm**), o sistema marca **ocupado** e acende o **LED verde**.
3. Se o **slot** configurado **não** bater com o **esperado**, acende o **LED vermelho** e o dado `fora_do_lugar` é enviado.
4. Se houver **presença** sem **slot** definido, acende o **LED azul** e `ocupacao_desconhecida = 1` é enviado.
5. Com **bateria baixa** (< 25%), acende o **LED laranja** e o campo correspondente é publicado.
6. Os dados aparecem no canal **Aurora-IoT** do ThingSpeak para consulta e histórico.

---

## Dashboards no ThingSpeak

* Configure **gráficos** para `dist_cm`, `presenca`, `bateria_pct`, `fora_do_lugar`, `ocupacao_desconhecida`.
* Configure **Last Value** para `slot` e `esperado` (melhor visual para texto).
* Opcional: crie **fórmulas**/**status** (ex.: gauge de bateria, contagem de vagas ocupadas/livres).

---

## Cenários de teste

1. **Bateria crítica**: gire o potenciômetro para < 25% → LED **laranja** e flag publicada.
2. **Ocupação**: aproxime a “moto” (reduza `dist_cm` < 20) → LED **verde**.
3. **Fora do lugar**: defina `slot esperado = A-12`, selecione `B-03` nos botões → LED **vermelho**.
4. **Ocupação desconhecida**: presença **sem slot** (use `clear`) → LED **azul**.
5. **Localizador**: envie `{"action":"beep","duration_ms":1500}` → buzzer + LED **amarelo**.

---

## Checklist Sprint 3 (IoT)

* [x] **3 sensores/atuadores**: ultrassom (presença), potenciômetro (bateria), botões (slot) + buzzer/LED (atuador).
* [x] **Comunicação em tempo real**: publicação periódica no **ThingSpeak** (~16 s).
* [x] **Dashboard**: gráficos/last values no canal (tempo real e histórico).
* [x] **Persistência**: armazenamento no **ThingSpeak** (histórico por campo).
* [x] **Cenários realistas**: fora do lugar, ocupação desconhecida, bateria baixa.

