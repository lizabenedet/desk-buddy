# Agente do PC

Manda para o buddy os eventos do Claude Code e as métricas do PC (CPU, RAM, GPU, temperaturas).

| Executável | Para quê |
|---|---|
| `buddy-tray.exe` | Agente na bandeja, sem janela. É o que inicia com o Windows. |
| `buddy.exe` | Linha de comando: `pair`, `hooks`, `autostart`, `run` (agente com logs no terminal) e `hook` (usado pelo Claude Code). |

## Compilar

```powershell
cd agent
cargo build --release      # target\release\buddy.exe e buddy-tray.exe
cargo test
```

## Configurar

1. **Parear** com o token da tela **Sistema** do buddy:
   ```powershell
   buddy pair K7Q2-M9XD                                  # placa: ws://buddy.local/ws
   buddy pair K7Q2-M9XD --url ws://127.0.0.1:8765/ws     # simulador
   ```
   A configuração fica em `%APPDATA%\desk-buddy\config.toml` (a variável `DESK_BUDDY_CONFIG_DIR` troca a pasta).

2. **Hooks do Claude Code**: `buddy hooks` imprime o bloco `"hooks"` para colocar em `~/.claude/settings.json`. O `buddy hook` não imprime nada e sempre sai com código 0, então não gasta tokens nem atrapalha o Claude, mesmo com o agente desligado.

3. **Iniciar com o Windows**: `buddy autostart on` (ou pelo menu da bandeja).

## GPU AMD e temperaturas (LibreHardwareMonitor)

Sem a LHM o agente manda só CPU e RAM; a tela PC do buddy mostra `--` no resto.

1. Baixe a [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases) e rode **como administrador** (sem isso não há temperaturas).
2. Em **Options**: marque **Remote Web Server → Run** (porta 8085), **Run On Windows Startup**, **Start Minimized** e **Minimize To Tray**.
3. Confira em `http://localhost:8085/data.json`. O menu da bandeja do agente mostra "LibreHardwareMonitor: ok".

O servidor web da LHM também responde na rede local; se não quiser isso, restrinja a porta 8085 no firewall do Windows.

## Logs

- `buddy run`: no terminal.
- `buddy-tray.exe`: `%APPDATA%\desk-buddy\agent.log`.
