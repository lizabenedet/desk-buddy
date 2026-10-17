# Fontes

`font_pt_20.c` — Montserrat SemiBold 20 px, 4 bpp, ASCII + Latin-1 (acentos do português), sem compressão. Licença: [OFL.txt](OFL.txt).

Gerada com:

```
npx lv_font_conv@1.5.3 --font Montserrat-SemiBold.ttf -r 0x20-0x7E -r 0xA0-0xFF --size 20 --bpp 4 --no-compress \
    --format lvgl --lv-include lvgl.h --lv-font-name font_pt_20 -o font_pt_20.c
```

TTF: https://github.com/JulietaUla/Montserrat/tree/master/fonts/ttf
