# Ligação do display

Fonte: esquemático da JC-ESP32P4-M3-DEV, conector **JP1 "Expand IO"** (2×13, 26 pinos, mesmo formato do header do Raspberry Pi).

## ⚠️ Não encaixe o display direto no JP1

O formato é igual ao do Raspberry Pi, mas a pinagem não. Encaixado direto, o display:

- liga o pino 17 do display (3,3 V) ao **GPIO20**;
- liga o pino 18 do display (LCD_RS, saída da placa) ao **VCC3V3** da placa (curto quando o firmware acionar o pino);
- aterra **C6_U0RXD** (pino 20) e **ES_I2C_SCL** (pino 25);
- usa **C6_IO9** (strapping de boot do C6) como LCD_CS e **C6_CHIP_PU** (reset do C6) como TP_CS — o Wi-Fi para de funcionar.

Use **jumpers fêmea-fêmea** seguindo a tabela abaixo.

## JP1 (placa)

| Pino | Sinal | Pino | Sinal |
|---|---|---|---|
| 1 | VCC3V3 | 2 | VCC5V |
| 3 | VCC3V3 | 4 | VCC5V |
| 5 | GND | 6 | GND |
| 7 | GPIO1 | 8 | — |
| 9 | GPIO2 | 10 | GPIO47 |
| 11 | GPIO3 | 12 | GPIO46 |
| 13 | GPIO4 | 14 | GND / GPIO45 (ambíguo no esquemático — não usar) |
| 15 | GPIO5 | 16 | GND |
| 17 | GPIO20 | 18 | VCC3V3 |
| 19 | GPIO32 | 20 | C6_U0RXD |
| 21 | GPIO33 | 22 | C6_U0TXD |
| 23 | ES_I2C_SDA | 24 | C6_IO9 |
| 25 | ES_I2C_SCL | 26 | C6_CHIP_PU |

Pinos 20–26 (lado direito) e 23/25 pertencem ao ESP32-C6 e ao codec de áudio: **não usar**.

## Ligação (8 sinais + energia)

| Sinal | Pino no display | → | Pino no JP1 | GPIO |
|---|---|---|---|---|
| 5 V | 2 | → | 2 | VCC5V |
| GND | 6 | → | 6 | GND |
| LCD_SCK / TP_SCK | 23 | → | 7 | GPIO1 |
| LCD_CS | 24 | → | 9 | GPIO2 |
| TP_CS | 26 | → | 11 | GPIO3 |
| LCD_RS (DC) | 18 | → | 13 | GPIO4 |
| RST | 22 | → | 15 | GPIO5 |
| TP_IRQ | 11 | → | 17 | GPIO20 |
| LCD_SI / TP_SI (MOSI) | 19 | → | 19 | GPIO32 |
| TP_SO (MISO) | 21 | → | 21 | GPIO33 |

O display regula seus 3,3 V a partir dos 5 V; o pino 1 do display (3,3 V) não precisa ser ligado.

LCD e touch compartilham SCK/MOSI/MISO; cada um tem seu CS.
