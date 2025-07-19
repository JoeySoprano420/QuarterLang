/QuarterCapsule/
│
├── src/
│   ├── main.qtr               # ⭑ Entry point
│   ├── settings.qtr           # ⭑ Core Settings Module
│   ├── ui/
│   │   ├── sdl_ui.qtr         # SDL2 frontend logic
│   │   ├── gtk_ui.qtr         # GTK3/4 bindings
│   │   └── webgl_ui.qtr       # WebGL runtime + WASM bindings
│   └── plugin/
│       └── PluginStoreFetcher.qtr
│
├── assets/
│   ├── themes/
│   ├── shaders/
│   └── manifest/
│
├── build/
│   ├── win64/
│   └── wasm/
│
├── thirdparty/
│   ├── sdl2/
│   ├── gtk/
│   └── emscripten/
│
├── scripts/
│   └── build_win64.sh
│   └── build_web.sh
│
└── manifest.qtr               # 🔍 Metadata + Plugins + UI Layout
