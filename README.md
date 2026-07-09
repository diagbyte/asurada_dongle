# Asurada ZMK Dongle

Cyber Formula **아스라다(Asurada)** 테마의 ZMK 동글 디스플레이 · A Cyber-Formula
**Asurada**-themed ZMK dongle display.

**[한국어](#한국어) · [English](#english)**

---

## 한국어

[carrefinho/prospector-zmk-module](https://github.com/carrefinho/prospector-zmk-module)
(`feat/new-status-screens`)에서 **크게 파생된**(heavily derived) 아스라다 테마의 ZMK
동글 디스플레이입니다. 위젯 프레임워크·폰트·배터리 위젯 등은 Prospector에서 왔고,
아스라다 레이아웃(3페이지 HUD·타코미터·구·눈·연결)은 새로 작성했습니다.
**Seeed XIAO nRF52840** 가 **Waveshare 1.28" 원형 LCD (GC9A01, 240×240)** 와
**CST816S** 정전식 터치를 구동하며, 기존 스플릿 키보드의 BLE **스플릿 센트럴("동글")**
역할을 합니다. **트랙볼은 선택**입니다.

화면은 **3페이지 캐러셀**이고, 화면을 **탭하면 다음 페이지로 순환**합니다:

1. **키보드** — 타이핑 속도 **타코미터**(자동차 계기판풍), 중앙에 큰 **레이어 이름**,
   **영문 모디파이어**(SHIFT/CTRL/ALT/GUI), 하단에 좌/우 반쪽 **배터리**
2. **트랙볼**(선택) — 굴러가는 **빨간 3D 구**, **SCROLL/SNIPE** 포인팅 모드 표시,
   트랙볼 **배터리**
3. **연결** — 스플릿 주변장치별 연결 점(초록/빨강) + 배터리 %

유휴 상태가 되면 화면이 꺼지는 대신 애니메이션 **아스라다 4눈** 스크린세이버로
전환됩니다(랜덤하게 두리번거리고 "TAP TO WAKE" 안내).

> **ZMK on Zephyr 4.1**(현재 `main`) 필요. 인트리 `galaxycore,gc9x01x` 디스플레이
> 드라이버와 `hynitron,cst816s` 터치 드라이버가 거기에만 있습니다.

### 범용 모듈

키맵은 **당신 것**입니다 — 아스라다 레이아웃은 이 저장소의 키맵에 하드코딩돼 있지
않습니다. WPM, 현재 레이어 이름(각 레이어의 `display-name`), 눌린 모디파이어, 반쪽별
배터리는 전부 실시간 ZMK 상태를 읽으므로, 어떤 키맵이든 그대로 표시됩니다. 트랙볼은
`CONFIG_ASURADA_TRACKBALL` 한 줄로 켜고 끕니다. 자세한 재사용법은 아래
[내 스플릿 키보드에서 재사용](#내-스플릿-키보드에서-재사용)을 참고하세요.

### Prospector 대비 변경점

| | Prospector | Asurada |
|---|---|---|
| 디스플레이 | ST7789V 240×280 (1.69") | **GC9A01 240×240 원형 (1.28")** |
| 터치 | — | **CST816S (I2C)** |
| 화면 구성 | 단일 상태 화면 | **3페이지 캐러셀(탭으로 순환)** |
| 타이핑 UI | 가로 WPM 바 | **자동차 타코미터**(8시→4시, 눈금·레드라인·WPM 숫자) |
| 레이어 UI | 롤러 / 점 | **중앙 큰 이름** |
| 포인팅 | — | **회전 3D 구 + SCROLL/SNIPE + 트랙볼 배터리**(선택) |
| 연결 | — | **주변장치별 점 + 배터리 페이지** |
| 유휴 | 백라이트 꺼짐 | **아스라다 4눈 스크린세이버** |

### 하드웨어 & 배선

XIAO nRF52840 핀(실크스크린 `Dn`):

| 신호 | 핀 | | 신호 | 핀 |
|---|---|---|---|---|
| **VCC** | **3V3** (3.3 V) | | **GND** | **GND** |
| LCD SCK / CLK | D8 (P1.13) | | LCD CS | D9 |
| LCD MOSI / DIN | D10 (P1.15) | | LCD DC | D7 |
| LCD RST | D3 | | LCD BL | D6 (PWM, P1.11) |
| Touch SDA | D4 (P0.04) | | Touch SCL | D5 (P0.05) |
| Touch INT | D2 | | Touch RST | D1 |

총 12선: 위 10개 신호 **+ VCC, GND**. 모듈 전원은 XIAO의 **3V3** 핀에서 받습니다(5 V
아님) — 공유 3.3 V 레귤레이터가 LCD·백라이트·터치를 함께 공급합니다. 신호 핀은
`boards/shields/asurada_adapter/boards/xiao_ble_zmk.overlay` 에 정의돼 있습니다.
**VCC/GND는 전원 레일이라 devicetree엔 없지만 반드시 연결해야 합니다.** 패널은 쓰기
전용이라 **MISO는 미사용**(D9를 CS로 쓰려고 D9에서 비켜 둠) — 연결하지 마세요.

### 설치

> 📎 **완전한 예시 설정** — Totem 키보드 + 이 동글 + Adept 트랙볼 — 은 이 모듈을
> 실제로 소비하는 [**diagbyte/zmk-config-totem**](https://github.com/diagbyte/zmk-config-totem)의
> **`dongle` 브랜치**를 참고하세요. (동글 없는 순수 스플릿은 `main` 브랜치.)

**키보드**의 `zmk-config` → `config/west.yml`에 이 모듈을 추가:

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: diagbyte                            # <-- 당신의 GitHub 계정/조직
      url-base: https://github.com/diagbyte
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main                            # Zephyr 4.1 라인
      import: app/west.yml
    - name: asurada_dongle                      # <-- 이 저장소
      remote: diagbyte
      revision: main
  self:
    path: config
```

`build.yaml`에서 동글을 이 쉴드로 빌드:

```yaml
include:
  - board: xiao_ble//zmk
    shield: [당신_키보드]_dongle asurada_adapter
```

레이어가 중앙에 뜨도록 키맵 레이어에 이름을 주세요:

```dts
keymap {
  compatible = "zmk,keymap";
  base { display-name = "Base"; bindings = < ... >; };
  nav  { display-name = "Nav";  bindings = < ... >; };
};
```

### 조작 (터치)

| 제스처 | 동작 |
|---|---|
| **탭** | 잠자면 깨우기 · 깨어 있으면 **다음 페이지로 순환** |
| 롱프레스(≥0.6초) | 지금 바로 스크린세이버(4눈) |
| (스와이프 상/하, 좌/우) | 코드엔 있으나 CST816S가 드래그를 잘 못 잡아 **불안정** |

> **참고:** CST816S 패널이 드래그 이동 좌표를 제대로 주지 않아 스와이프가 잘 동작하지
> 않습니다. 그래서 페이지 이동은 **탭 순환**과 **자동 페이지 전환**
> (`CONFIG_ASURADA_AUTO_PAGE_FOLLOW`, 기본 켜짐 — 트랙볼 굴리면 트랙볼 페이지, 타이핑하면
> 키보드 페이지)이 주 수단입니다. 밝기는 현재 `CONFIG_ASURADA_FIXED_BRIGHTNESS`로
> 고정됩니다.

### 설정 (`.conf`)

**화면·터치·유휴**

| 옵션 | 기본 | 설명 |
|---|---|---|
| `CONFIG_ASURADA_FIXED_BRIGHTNESS` | 60 | 백라이트 밝기(1–100) |
| `CONFIG_ASURADA_TOUCH` | y | CST816S 터치 + 제스처 |
| `CONFIG_ASURADA_AUTO_PAGE_FOLLOW` | y | 입력 장치에 맞춰 페이지 자동 전환 |
| `CONFIG_ASURADA_SCREENSAVER` | y | 4눈 유휴 화면 |
| `CONFIG_ASURADA_SCREENSAVER_BRIGHTNESS` | 15 | 유휴 시 밝기 |
| `CONFIG_ASURADA_LAYER_NAME_UPPERCASE` | y | 레이어 이름 대문자 |
| `CONFIG_ASURADA_ROTATE_DISPLAY_180` | n | 거꾸로 장착 시 뒤집기 |
| `CONFIG_ZMK_IDLE_TIMEOUT` | 30000 | 유휴→스크린세이버 진입(ms) |

**트랙볼 (선택)**

| 옵션 | 기본 | 설명 |
|---|---|---|
| `CONFIG_ASURADA_TRACKBALL` | y | 트랙볼 페이지·입력·위젯. `n`이면 키보드 전용(2페이지) |
| `CONFIG_ASURADA_TRACKBALL_SLOT` | 2 | 트랙볼의 스플릿 슬롯 번호 |
| `CONFIG_ASURADA_SCROLL_LAYER` / `_LAYER2` | 6 / 7 | "SCROLL"로 표시할 레이어 |
| `CONFIG_ASURADA_SNIPE_LAYER` | 8 | "SNIPE"로 표시할 레이어 |

**연결 페이지 라벨**

| 옵션 | 기본 |
|---|---|
| `CONFIG_ASURADA_CONN_LABEL_0` / `_1` / `_2` / `_3` | Left / Right / Trackball / Periph 4 |

### 내 스플릿 키보드에서 재사용

- **트랙볼 없음?** `CONFIG_ASURADA_TRACKBALL=n` → 키보드 전용 동글(키보드·연결 2페이지),
  트랙볼 코드(구·포인팅·배터리)는 아예 빌드 안 함.
- **트랙볼 있음?** `CONFIG_ASURADA_TRACKBALL_SLOT`을 트랙볼 슬롯에 맞추고,
  `CONFIG_ASURADA_SCROLL_LAYER` / `_LAYER2` / `CONFIG_ASURADA_SNIPE_LAYER`(기본 6/7/8)를
  당신 키맵의 스크롤/스나이프 레이어 번호로 지정하면 트랙볼 페이지 포인팅 모드 표시가
  따라갑니다.
- **연결 페이지**는 스플릿 주변장치 수(`ZMK_SPLIT_BLE_PERIPHERAL_COUNT`, 최대 4행)만큼
  행을 그리고, `CONFIG_ASURADA_CONN_LABEL_0..3`로 라벨을 붙입니다.
- 키보드 페이지(WPM·레이어명·모디파이어·좌우 배터리)는 실시간 ZMK 상태라 별도 설정
  없이 동작합니다.

### 펌웨어 구조

```
Kconfig                                    ASURADA_* 옵션
boards/shields/asurada_adapter/
  boards/xiao_ble_zmk.overlay              GC9A01 + CST816S 노드
  Kconfig.defconfig                        디스플레이/LVGL 기본값
  src/touch.c                              CST816S 제스처(탭=페이지 순환)
  src/screensaver.c                        4눈 유휴 화면 + TAP TO WAKE
  src/screens.c                            페이지 캐러셀 매니저
  src/page_follow.c                        입력 장치 따라 페이지 자동 전환
  src/trackball_input.c                    전달된 포인터 이동 수집(구 애니메이션용)
  src/layouts/asurada/
    status_screen.c                        3페이지 캐러셀 조립
    wpm_border.{c,h}                        타코미터(아크+눈금+레드라인+WPM 숫자)
    layer_center.{c,h}                      중앙 레이어 이름(키맵 display-name)
    modifiers.{c,h}                         SHIFT/CTRL/ALT/GUI 표시
    battery_circles.{c,h}                   좌/우 반쪽 배터리
    ball.{c,h}                              빨간 회전 3D 구(DRAW_MAIN)
    pointing_mode.{c,h}                     SCROLL/SNIPE 표시
    trackball_battery.{c,h}                 트랙볼 배터리
    connections.{c,h}                       주변장치별 연결 + 배터리 페이지
    display_colors.h                        아스라다 팔레트
```

### 검증 · 3D 프린팅 · 크레딧

- 빌드 + 온디바이스 체크리스트: `docs/VERIFY.md`
- 기구부: `hardware/asurada_mount.scad`(파라메트릭 코어 — LCD 베젤/XIAO 트레이/스탠드오프),
  `hardware/README.md`
- **Prospector**(carrefinho, [hardware](https://github.com/carrefinho/prospector) ·
  [firmware](https://github.com/carrefinho/prospector-zmk-module))를 **기반으로 크게
  개조**했으며, 그 원류는 englmaxi의 `zmk-dongle-display`입니다. 위젯 프레임워크·폰트·
  battery 위젯은 Prospector 파생이고, Prospector의 classic/radii/field/operator
  레이아웃은 이 저장소에서 제거했습니다. `LICENSE` 표기대로 CERN-OHL-P-2.0 / MIT.

---

## English

An Asurada-themed ZMK dongle display, **heavily derived from**
[carrefinho/prospector-zmk-module](https://github.com/carrefinho/prospector-zmk-module)
(`feat/new-status-screens`) — the widget framework, fonts and battery widget come
from Prospector; the Asurada layout (3-page HUD, tachometer, ball, eyes, connections)
is new. A **Seeed XIAO nRF52840** drives a **Waveshare 1.28"
round LCD (GC9A01, 240×240)** with a **CST816S** capacitive touch panel, and acts
as the BLE **split central ("dongle")** for an existing split keyboard. A
**trackball is optional**.

The screen is a **3-page carousel**; **tap the screen to cycle to the next page**:

1. **Keyboard** — a typing-speed **tachometer** (automotive-instrument style), the
   current **layer name** large in the center, an **English modifier** indicator
   (SHIFT/CTRL/ALT/GUI), and left/right half **battery** at the bottom.
2. **Trackball** (optional) — a procedural rolling **red 3D sphere**, **SCROLL/SNIPE**
   pointing-mode text, and the trackball **battery**.
3. **Connections** — one row per split peripheral: a green/red connect dot + battery %.

When idle, instead of blanking the display switches to the animated **Asurada
four-eyes** screensaver (idle glances + a "TAP TO WAKE" hint).

> **Requires ZMK on Zephyr 4.1** (current `main`). The in-tree `galaxycore,gc9x01x`
> display driver and `hynitron,cst816s` touch driver only exist there.

### Generic / reusable

The keymap is **yours** — nothing on the asurada layout is hard-wired to this repo's
keymap. WPM, the current layer's name (via each layer's `display-name`), held
modifiers, and per-half battery all read live ZMK state, so any keymap just works.
The trackball is toggled by a single `CONFIG_ASURADA_TRACKBALL`. See
[Reusing on your own keyboard](#reusing-on-your-own-split-keyboard).

### What changed vs. Prospector

| | Prospector | Asurada |
|---|---|---|
| Display | ST7789V 240×280 (1.69") | **GC9A01 240×240 round (1.28")** |
| Touch | — | **CST816S (I2C)** |
| Layout | single status screen | **3-page carousel (tap to cycle)** |
| Typing UI | horizontal WPM bar | **automotive tachometer** (8→4 o'clock, ticks, redline, WPM number) |
| Layer UI | roller / dots | **large centered name** |
| Pointing | — | **rolling 3D ball + SCROLL/SNIPE + trackball battery** (optional) |
| Connections | — | **per-peripheral dot + battery page** |
| Idle | blank backlight | **Asurada four-eyes screensaver** |

### Hardware & wiring

XIAO nRF52840 pins (silkscreen `Dn`):

| Signal | Pin | | Signal | Pin |
|---|---|---|---|---|
| **VCC** | **3V3** (3.3 V) | | **GND** | **GND** |
| LCD SCK / CLK | D8 (P1.13) | | LCD CS | D9 |
| LCD MOSI / DIN | D10 (P1.15) | | LCD DC | D7 |
| LCD RST | D3 | | LCD BL | D6 (PWM, P1.11) |
| Touch SDA | D4 (P0.04) | | Touch SCL | D5 (P0.05) |
| Touch INT | D2 | | Touch RST | D1 |

12 wires total: the 10 signals above **plus VCC and GND**. Power the module from the
XIAO's **3V3** pin (not 5 V); the shared 3.3 V regulator supplies the LCD, backlight
and touch. Signal pins are defined in
`boards/shields/asurada_adapter/boards/xiao_ble_zmk.overlay`. **VCC and GND are power
rails, so they are not in the devicetree — but they are required.** The panel is
write-only, so **MISO is unused** (parked off D9 so D9 can serve as CS); leave it
unconnected.

### Installation

> 📎 **Complete example config** — the Totem keyboard + this dongle + an Adept
> trackball — lives in the **`dongle` branch** of
> [**diagbyte/zmk-config-totem**](https://github.com/diagbyte/zmk-config-totem), a
> real config that consumes this module. (The `main` branch is the plain
> dongle-less split.)

In your **keyboard's** `zmk-config`, add this module to `config/west.yml`:

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: diagbyte                            # <-- your GitHub user/org
      url-base: https://github.com/diagbyte
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main                            # Zephyr 4.1 line
      import: app/west.yml
    - name: asurada_dongle                      # <-- this repo
      remote: diagbyte
      revision: main
  self:
    path: config
```

Then build the dongle with the shield in `build.yaml`:

```yaml
include:
  - board: xiao_ble//zmk
    shield: [YOUR_KEYBOARD]_dongle asurada_adapter
```

Give your keymap layers names so they show in the center:

```dts
keymap {
  compatible = "zmk,keymap";
  base { display-name = "Base"; bindings = < ... >; };
  nav  { display-name = "Nav";  bindings = < ... >; };
};
```

### Navigation & touch

| Gesture | Action |
|---|---|
| **Tap** | Wake when asleep · **cycle to the next page** when awake |
| Long press (≥0.6 s) | Show the four-eyes screensaver now |
| (Swipe up/down, left/right) | Present in code, but **unreliable** — see note |

> **Note:** the CST816S panel doesn't report drag motion reliably, so swipe gestures
> mostly don't fire. Page navigation is therefore **tap-to-cycle** plus **auto page
> follow** (`CONFIG_ASURADA_AUTO_PAGE_FOLLOW`, on by default — roll the trackball →
> trackball page, type → keyboard page). Brightness is currently fixed via
> `CONFIG_ASURADA_FIXED_BRIGHTNESS`.

### Configuration (`.conf`)

**Display · touch · idle**

| Option | Default | Notes |
|---|---|---|
| `CONFIG_ASURADA_FIXED_BRIGHTNESS` | 60 | Backlight (1–100) |
| `CONFIG_ASURADA_TOUCH` | y | CST816S touch + gestures |
| `CONFIG_ASURADA_AUTO_PAGE_FOLLOW` | y | Auto-switch page to the active input |
| `CONFIG_ASURADA_SCREENSAVER` | y | Four-eyes idle screen |
| `CONFIG_ASURADA_SCREENSAVER_BRIGHTNESS` | 15 | Dim level while idle |
| `CONFIG_ASURADA_LAYER_NAME_UPPERCASE` | y | Uppercase layer names |
| `CONFIG_ASURADA_ROTATE_DISPLAY_180` | n | Flip if mounted upside down |
| `CONFIG_ZMK_IDLE_TIMEOUT` | 30000 | ms before idle → screensaver |

**Trackball (optional)**

| Option | Default | Notes |
|---|---|---|
| `CONFIG_ASURADA_TRACKBALL` | y | Trackball page/input/widgets. `n` → keyboard-only (2 pages) |
| `CONFIG_ASURADA_TRACKBALL_SLOT` | 2 | The trackball's split-peripheral slot |
| `CONFIG_ASURADA_SCROLL_LAYER` / `_LAYER2` | 6 / 7 | Layers shown as "SCROLL" |
| `CONFIG_ASURADA_SNIPE_LAYER` | 8 | Layer shown as "SNIPE" |

**Connections labels**

| Option | Default |
|---|---|
| `CONFIG_ASURADA_CONN_LABEL_0` / `_1` / `_2` / `_3` | Left / Right / Trackball / Periph 4 |

### Reusing on your own split keyboard

- **No trackball?** Set `CONFIG_ASURADA_TRACKBALL=n` for a keyboard-only dongle: the
  carousel becomes two pages (keyboard, connections) and none of the trackball code
  (rolling ball, pointing-mode text, trackball battery) is built.
- **Have a trackball?** Point `CONFIG_ASURADA_TRACKBALL_SLOT` at its split slot, and
  set `CONFIG_ASURADA_SCROLL_LAYER` / `_LAYER2` / `CONFIG_ASURADA_SNIPE_LAYER`
  (default 6/7/8) to your own keymap's scroll/snipe layer indices so the pointing-mode
  text tracks your layers.
- **Connections page** shows one row per split peripheral
  (`ZMK_SPLIT_BLE_PERIPHERAL_COUNT`, up to 4 rows), labeled from
  `CONFIG_ASURADA_CONN_LABEL_0` through `..._3`.
- The keyboard page (WPM, layer name, modifiers, L/R battery) reads live ZMK state and
  needs no configuration.

### Firmware map

```
Kconfig                                    ASURADA_* options
boards/shields/asurada_adapter/
  boards/xiao_ble_zmk.overlay              GC9A01 + CST816S nodes
  Kconfig.defconfig                        display / LVGL defaults
  src/touch.c                              CST816S gestures (tap cycles pages)
  src/screensaver.c                        four-eyes idle screen + TAP TO WAKE
  src/screens.c                            paged carousel manager
  src/page_follow.c                        auto-switch page to the active input
  src/trackball_input.c                    taps forwarded pointer motion (for the ball)
  src/layouts/asurada/
    status_screen.c                        assembles the 3-page carousel
    wpm_border.{c,h}                        tachometer gauge (arc + ticks + redline + WPM)
    layer_center.{c,h}                      centered layer name (keymap display-name)
    modifiers.{c,h}                         SHIFT/CTRL/ALT/GUI indicator
    battery_circles.{c,h}                   left/right half battery
    ball.{c,h}                              rolling red 3D sphere (DRAW_MAIN)
    pointing_mode.{c,h}                     SCROLL/SNIPE text
    trackball_battery.{c,h}                 trackball battery
    connections.{c,h}                       per-peripheral connect + battery page
    display_colors.h                        Asurada palette
```

### Verification · 3D printing · credits

- Build + on-device checklist: `docs/VERIFY.md`
- Mechanical: `hardware/asurada_mount.scad` (parametric core — LCD bezel, XIAO tray,
  standoffs), `hardware/README.md`
- **Based on / heavily derived from Prospector** by carrefinho
  ([hardware](https://github.com/carrefinho/prospector),
  [firmware](https://github.com/carrefinho/prospector-zmk-module)), itself derived
  from englmaxi's `zmk-dongle-display`. The widget framework, fonts and battery
  widget are Prospector-derived; Prospector's classic/radii/field/operator layouts
  were removed from this repo. Licensed CERN-OHL-P-2.0 / MIT as noted in `LICENSE`.
