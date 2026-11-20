set -e
em++ -std=c++17 -O3 -I. webui/wasm_bridge.cpp \
  -lembind \
  -sMODULARIZE=1 \
  -sEXPORT_NAME=createOptimaModule \
  -sALLOW_MEMORY_GROWTH=1 \
  -o webui/optima.js