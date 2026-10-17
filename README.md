# desk-buddy

Buddy de mesa (ESP32-P4 + display 3,5") que reage ao PC e ao Claude Code. Design em [docs/design.md](docs/design.md), ligação do display em [docs/wiring.md](docs/wiring.md).

## Simulador (Windows)

```powershell
firmware\sim\build.ps1        # compila e roda os testes
firmware\sim\build.ps1 -Run   # ... e abre o simulador (mouse = toque, arraste para trocar de tela)
```

Teclas no simulador: `0`–`9` e `-` trocam o humor, `r` faz uma reação feliz curta, `s` mostra o balão de fala, `w` troca o clima falso, `n` liga/desliga o Wi-Fi, `a` dispara um alerta. Clique rápido no rosto = cutucar, segurar = carinho.

`desk_buddy_sim.exe --shots <pasta>` salva cada humor e cada tela como BMP (sem capturar a tela do PC).

O simulador também aceita o agente do PC em `ws://127.0.0.1:8765/ws` (o token aparece no console e na tela **Sistema**); `--trace` imprime cada evento. Protocolo: [protocol/README.md](protocol/README.md).

## Firmware

Num terminal "ESP-IDF PowerShell" (ou após `. C:\Espressif\tools\Microsoft.v6.0.3.PowerShell_profile.ps1`):

```powershell
cd firmware
idf.py menuconfig              # desk-buddy -> Wi-Fi SSID/senha (ficam só no sdkconfig local)
idf.py build
idf.py -p COMx flash monitor
```

Cidade padrão do clima: Jaraguá do Sul (lat/lon e fuso também no menu `desk-buddy`).

## Agente do PC

Em Rust, na pasta `agent/`: bandeja + início com o Windows, métricas (CPU/RAM; GPU e temperaturas via LibreHardwareMonitor) e os hooks do Claude Code. Instruções em [agent/README.md](agent/README.md).

## Adicionando uma feature

1. Crie `firmware/components/app/modules/<nome>.c` com um `module_t` (veja `clock.c`).
2. Liste-o em `firmware/components/app/modules.c`.

Os builds pegam todos os `.c` das pastas automaticamente.
