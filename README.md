# desk-buddy

Buddy de mesa (ESP32-P4 + display 3,5") que reage ao PC e ao Claude Code. Design em [docs/design.md](docs/design.md), ligação do display em [docs/wiring.md](docs/wiring.md).

## Simulador (Windows)

```powershell
firmware\sim\build.ps1        # compila e roda os testes
firmware\sim\build.ps1 -Run   # ... e abre o simulador (mouse = toque, arraste para trocar de tela)
```

## Firmware

Num terminal "ESP-IDF PowerShell" (ou após `. C:\Espressif\tools\Microsoft.v6.0.3.PowerShell_profile.ps1`):

```powershell
cd firmware
idf.py build
idf.py -p COMx flash monitor
```

## Adicionando uma feature

1. Crie `firmware/components/app/modules/<nome>.c` com um `module_t` (veja `clock.c`).
2. Liste-o em `firmware/components/app/modules.c`.
3. Adicione o arquivo em `firmware/components/app/CMakeLists.txt` e `firmware/sim/CMakeLists.txt`.
