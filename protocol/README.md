# Protocolo PC ↔ buddy

WebSocket, mensagens de texto JSON (UTF-8), um objeto por mensagem.

| | Endereço |
|---|---|
| Placa | `ws://buddy.local/ws` (porta 80, anunciada por mDNS) |
| Simulador | `ws://127.0.0.1:8765/ws` |

Só um agente por vez: um novo `hello` válido derruba a conexão anterior.

## Autenticação

A primeira mensagem do agente precisa ser `hello` com o token mostrado na tela **Sistema** do buddy. Qualquer outra mensagem antes disso, ou token errado, recebe `error` e a conexão é fechada.

```json
{"type": "hello", "version": 1, "token": "K7Q2-M9XD"}
```

Respostas da placa:

```json
{"type": "welcome", "version": 1}
{"type": "error", "reason": "bad_token"}      // ou "not_authenticated", "bad_message", "unsupported_version"
```

## Agente → buddy

### `claude` — hooks do Claude Code

```json
{"type": "claude", "event": "waiting", "project": "desk-buddy"}
```

| `event` | Hook | Reação |
|---|---|---|
| `session_start` | `SessionStart` | acorda, "Oi!" |
| `prompt` | `UserPromptSubmit` | pensando |
| `waiting` | `Notification` | alerta âmbar + "*projeto*: esperando você" até o próximo evento |
| `done` | `Stop` | feliz + "*projeto*: pronto!" |
| `session_end` | `SessionEnd` | volta a descansar |

`project` é opcional.

### `metrics` — estado do PC (a cada ~2 s)

```json
{"type": "metrics", "cpu": 37.5, "ram": 61.2, "gpu": 12.0, "cpu_temp": 55.0, "gpu_temp": 48.0}
```

Todos os campos são opcionais (ex.: sem LibreHardwareMonitor não há `gpu`/temperaturas). Percentuais 0–100, temperaturas em °C.

### `pc` — mudanças de estado do PC

```json
{"type": "pc", "state": "sleep"}
```

`state`: `sleep` (vai suspender/desligar).

### `ping`

```json
{"type": "ping"}
```

Mantém a conexão viva. Sem nenhuma mensagem por 45 s, a placa considera o PC desconectado; o agente deve mandar `ping` (ou `metrics`) pelo menos a cada 15 s.

## Buddy → agente

Só `welcome` e `error` (acima) por enquanto.
