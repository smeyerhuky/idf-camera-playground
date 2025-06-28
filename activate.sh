#!/bin/bash
# ESP-IDF Camera Development Environment Activation Script

SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
ESP_IDF_PATH="$SCRIPT_DIR/esp-idf"

if [ ! -d "$ESP_IDF_PATH" ]; then
    echo "❌ ESP-IDF not found at $ESP_IDF_PATH"
    echo "Run ./prepare.sh first to set up the development environment"
    return 1
fi

echo "🚀 Activating ESP-IDF Camera Development Environment..."
source "$ESP_IDF_PATH/export.sh"

# Set additional environment variables
export ESP_WHO_PATH="$SCRIPT_DIR/esp-who"
export ESP_PROJECTS_PATH="$SCRIPT_DIR/projects"

echo "✅ Environment activated!"
echo "📁 ESP-IDF Path: $IDF_PATH"
echo "📁 ESP-WHO Path: $ESP_WHO_PATH"
echo "📁 Projects Path: $ESP_PROJECTS_PATH"
echo ""
echo "🔧 Quick commands:"
echo "  ./build.sh list              # List available projects"
echo "  ./build.sh build <project>   # Build a project"
echo "  idf.py --help               # ESP-IDF help"
echo ""
