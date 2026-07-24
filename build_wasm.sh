#!/bin/bash
# Exit on any error
set -e

# Find script directory and project root
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR"

echo "=== Building Primer3 Thermodynamic Engine to WebAssembly ==="

# Check if emcc is installed
if ! command -v emcc &> /dev/null; then
    echo "❌ Error: 'emcc' (Emscripten Compiler) is not found."
    echo "Please install Emscripten (emsdk) and activate it in your terminal first."
    echo "Instructions:"
    echo "  1. git clone https://github.com/emscripten-core/emsdk.git"
    echo "  2. cd emsdk && ./emsdk install latest && ./emsdk activate latest"
    echo "  3. source ./emsdk_env.sh"
    exit 1
fi

# Ensure src/ directory exists
if [ ! -d "$PROJECT_ROOT/src" ]; then
    echo "❌ Error: Cannot find 'src/' directory. Run this script from the root of primer3 repository."
    exit 1
fi

# Locate primer3_config directory
CONFIG_DIR="$PROJECT_ROOT/src/primer3_config"
if [ ! -d "$CONFIG_DIR" ]; then
    echo "❌ Error: Cannot find 'src/primer3_config/' directory."
    exit 1
fi

echo "📦 Found parameter directory: $CONFIG_DIR"
echo "🛠️ Compiling WASM module..."

# Compile
emcc -Oz \
    "$PROJECT_ROOT/src/wasm_api.c" \
    "$PROJECT_ROOT/src/thal.c" \
    "$PROJECT_ROOT/src/oligotm.c" \
    -I"$PROJECT_ROOT/src" \
    --embed-file "$CONFIG_DIR@/primer3_config" \
    -s EXPORTED_FUNCTIONS="['_wasm_oligotm','_wasm_hairpin','_wasm_dimer','_malloc','_free']" \
    -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap','stringToUTF8','UTF8ToString']" \
    -s MODULARIZE=1 \
    -s EXPORT_NAME="createPrimer3Thermo" \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s NO_EXIT_RUNTIME=1 \
    -s ENVIRONMENT="web,webview,worker,node" \
    -o "$PROJECT_ROOT/primer3_thermo.js"

echo "✅ Success! Generated:"
echo "  - $PROJECT_ROOT/primer3_thermo.js"
echo "  - $PROJECT_ROOT/primer3_thermo.wasm"
echo ""
echo "📝 Usage Example in JavaScript:"
echo "--------------------------------------------------------"
echo "import createPrimer3Thermo from './primer3_thermo.js';"
echo ""
echo "const Module = await createPrimer3Thermo();"
echo ""
echo "// 1. Oligo Tm Calculation"
echo "const oligotm = Module.cwrap('wasm_oligotm', 'string', ["
echo "  'string', 'double', 'double', 'double', 'double', 'double', 'double', 'double', 'number', 'number', 'number', 'double'"
echo "]);"
echo "const tmResult = JSON.parse(oligotm('ATCGATCGATCGATCG', 50.0, 50.0, 1.5, 0.6, 0.0, 0.6, 0.0, 60, 1, 1, 50.0));"
echo "console.log('Oligo Tm Result:', tmResult);"
echo ""
echo "// 2. Hairpin Secondary Structure Calculation"
echo "const hairpin = Module.cwrap('wasm_hairpin', 'string', ["
echo "  'string', 'double', 'double', 'double', 'double', 'double'"
echo "]);"
echo "const hpResult = JSON.parse(hairpin('GCGCGCGCGCGCGCAAAAAAGCGCGCGCGCGCGC', 50.0, 0.0, 0.0, 50.0, 37.0));"
echo "console.log('Hairpin Result:', hpResult);"
echo "console.log(hpResult.structure);"
echo "--------------------------------------------------------"
