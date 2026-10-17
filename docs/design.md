# desk-buddy — Documento de design

Buddy de mesa com rosto animado que reage ao que acontece no PC (Claude Code, CPU, GPU, temperaturas) e continua útil com o PC desligado (relógio, clima, timer, pet).

## Princípios

- **Resolver os problemas de agora.** Sem abstrações para cenários hipotéticos.
- **A placa é autônoma.** A inteligência vive no firmware; o PC é só mais uma fonte de eventos.
- **Feature nova = módulo novo.** Módulos se comunicam por um barramento de eventos e registram suas próprias telas e campos de configuração.

## Hardware

| Item | Detalhe |
|---|---|
| Placa | Guition JC-ESP32P4-M3 — ESP32-P4 (RISC-V 2×400 MHz, 32 MB PSRAM, 16 MB flash) + ESP32-C6 (Wi-Fi 6/BT5 via `esp-hosted`/SDIO) |
| Display | 3.5" RPi Display 480×320 — ILI9486 + toque resistivo XPT2046, SPI compartilhado |
| Ligação | Jumpers do header GPIO da placa para o header RPi de 26 pinos do display: MOSI, MISO, SCK, LCD_CS, TP_CS, DC, RST, TP_IRQ, 5V, GND |
| Som | Nenhum (sem alto-falante). Alertas são visuais. |
| Energia | Carregador de tomada (para funcionar com o PC desligado) |

Limitação: SPI estável em ~16–20 MHz → ~6–8 fps em tela cheia. Animações devem ficar em regiões pequenas (LVGL redesenha só áreas sujas).

## Arquitetura

```
┌──────────────── PC (Windows) ────────────────┐        ┌──────────────── Placa (ESP32-P4) ────────────────┐
│ Claude Code hooks ──► buddy.exe send ──┐     │        │  transport (WebSocket) ──► event bus ◄──┐        │
│                                        ▼     │  Wi-Fi │                               │         │        │
│ sources: sysinfo, LHM ──► agente (bandeja) ──┼───────►│   modules: clock, weather, timer, pet, pc_link   │
│                                              │ WS+JSON│                               │                  │
└──────────────────────────────────────────────┘        │   ui: rosto procedural + telas (LVGL 480×320)    │
                                                        └──────────────────────────────────────────────────┘
```

### Firmware (C, ESP-IDF + LVGL)

- **core** — barramento de eventos, registro de módulos, configuração (NVS), lógica pura testável no PC.
- **ui** — rosto procedural (olhos/boca como parâmetros interpolados), barra de status, telas por swipe, balão de fala. Fixo em 480×320.
- **hal_esp** — display ILI9486, toque XPT2046, Wi-Fi, mDNS, NVS, OTA.
- **modules**
  - `clock` — NTP, fuso configurável.
  - `weather` — Open-Meteo (sem chave), lat/lon configuráveis.
  - `timer` — timer, Pomodoro, alarmes (alerta visual).
  - `pet` — medidores **energia** e **felicidade** persistidos na NVS; influenciam a expressão padrão; sem punição. Toque no rosto = carinho/cutucar. Comportamento ambiente: piscar, olhar, tédio, dormir à noite.
  - `pc_link` — recebe eventos do agente; mostra buddy "sozinho/dormindo" quando o PC desconecta.

Cada módulo registra: handlers de eventos, opcionalmente uma tela de swipe e campos de configuração (que aparecem na página `buddy.local`).

**Telas (layout fixo):** rosto como tela principal com barra de status no topo (hora, clima, Wi-Fi/PC, timer ativo); swipe para relógio/clima detalhado, timer/alarmes, status do PC, configurações.

**Configuração:** Wi-Fi via `menuconfig` (v1); demais opções na página web `http://buddy.local`. Portal `Buddy-Setup` (SoftAP) fica para v1.1, após validar AP via `esp-hosted`.

**OTA** via Wi-Fi pela página `buddy.local`.

**Segurança:** token compartilhado gerado no primeiro boot (exibido na tela de configurações), exigido na conexão WebSocket; senha para página web e OTA. Sem TLS.

### Comunicação

- WebSocket servido pela placa, anunciado por mDNS como `buddy.local`.
- Mensagens JSON. Camada de transporte separada (USB serial pode ser adicionada depois).
- Especificação única em `protocol/`.

### Agente do PC (Rust)

- Roda na bandeja e inicia com o Windows; `buddy.exe run --console` para desenvolvimento.
- Mantém uma conexão com a placa (fila, reconexão, debounce).
- Fontes (trait `EventSource`):
  - `sysinfo` — CPU %, RAM %.
  - LibreHardwareMonitor via `http://localhost:8085/data.json` — GPU AMD (uso, VRAM, temperatura) e temperaturas de CPU. Se indisponível, segue sem elas.
- `buddy.exe send <evento>` — entrada para hooks do Claude Code. **Nunca imprime nada e sempre sai com código 0** (custo zero de tokens, nunca bloqueia o Claude).
- Envia `pc_sleep` ao suspender/desligar quando possível.

**Hooks do Claude Code (v1):** `UserPromptSubmit` (pensando), `Notification` (esperando você — alerta forte), `Stop` (pronto), `SessionStart`/`SessionEnd` (acorda/descansa). Cada evento leva o nome do projeto.

**Reações a métricas (limites configuráveis):** CPU alta > 85% por 30 s → suado/cansado (gasta energia); temp > ~80 °C → com calor; > ~90 °C → alerta.

## Desenvolvimento

- **Simulador** LVGL + SDL2 no Windows, compartilhando `core` e `ui` com o firmware.
- **Testes unitários** da lógica (pet, protocolo, timer, regras) no PC; `cargo test` no agente.
- Código, identificadores e commits em inglês; docs e textos da tela em PT-BR.

## Estrutura do repositório

```
desk-buddy/
├── firmware/
│   ├── main/
│   ├── components/{core,ui,hal_esp,modules}
│   └── sim/
├── agent/
├── protocol/
└── docs/
```

## Fases

1. Setup do ambiente (ESP-IDF, Rust, Build Tools, SDL2).
2. Bring-up: pinagem do header, display + toque com LVGL. **Maior risco técnico.**
3. Núcleo: barramento de eventos, módulos, simulador.
4. Rosto procedural + máquina de humor.
5. Módulos offline: relógio, clima, timer, pet.
6. Comunicação: WebSocket, protocolo, token, mDNS.
7. Agente Rust: CPU/RAM → LHM → `buddy send` + hooks.
8. Página `buddy.local` + OTA.

## Fora da v1

Home Assistant/MQTT, voz, som, portal Wi-Fi, teclado/mouse/janela em foco, múltiplas sessões do Claude.

## Riscos a verificar

- Pinagem exata do header GPIO da placa.
- Driver ILI9486 para `esp_lcd` e variante do display (com/sem registrador de deslocamento).
- Modo AP do `esp-hosted` no C6.
