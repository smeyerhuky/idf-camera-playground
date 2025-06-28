#!/bin/bash

# Hummingbird Repository Master Build System
# Central project registry and build automation

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Repository configuration
REPO_ROOT="$(dirname "$(readlink -f "$0")")"
ESP_IDF_PATH="$REPO_ROOT/esp-idf"
ESP_WHO_PATH="$REPO_ROOT/esp-who"
ESP_RAINMAKER_PATH="$REPO_ROOT/esp-rainmaker"
ESP_PROJECTS_PATH="$REPO_ROOT/projects"

# Auto-discovered project storage
declare -A PROJECTS=()
declare -A PROJECT_DESCRIPTIONS=()
declare -A PROJECT_TARGETS=()
declare -A PROJECT_FEATURES=()

# Discover and load all projects with metadata
discover_projects() {
    print_status "Discovering projects in $ESP_PROJECTS_PATH..."
    
    # Clear existing data
    PROJECTS=()
    PROJECT_DESCRIPTIONS=()
    PROJECT_TARGETS=()
    PROJECT_FEATURES=()
    
    for project_dir in "$ESP_PROJECTS_PATH"/*; do
        if [ -d "$project_dir" ]; then
            local project_name="$(basename "$project_dir")"
            local metadata_file="$project_dir/project.json"
            
            if [ -f "$metadata_file" ]; then
                # Parse JSON metadata using basic shell tools
                local description=$(grep '"description"' "$metadata_file" | sed 's/.*"description":[[:space:]]*"\([^"]*\)".*/\1/')
                local target=$(grep '"target"' "$metadata_file" | sed 's/.*"target":[[:space:]]*"\([^"]*\)".*/\1/')
                local features=$(grep -A 10 '"features"' "$metadata_file" | grep '".*"' | sed 's/.*"\([^"]*\)".*/\1/' | tr '\n' ', ' | sed 's/,$//')
                
                # Register project
                PROJECTS["$project_name"]="$project_dir"
                PROJECT_DESCRIPTIONS["$project_name"]="$description"
                PROJECT_TARGETS["$project_name"]="$target"
                PROJECT_FEATURES["$project_name"]="$features"
                
                print_status "Found project: $project_name ($target)"
            else
                print_warning "Skipping $project_name (no project.json metadata)"
            fi
        fi
    done
    
    if [ ${#PROJECTS[@]} -eq 0 ]; then
        print_warning "No projects with metadata found in $ESP_PROJECTS_PATH"
    else
        print_status "Discovered ${#PROJECTS[@]} project(s)"
    fi
}

# Function to print formatted output
print_header() {
    echo -e "${BOLD}${BLUE}================================================${NC}"
    echo -e "${BOLD}${BLUE}  🐦 Repository Build System${NC}"
    echo -e "${BOLD}${BLUE}================================================${NC}"
}

print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_project() {
    echo -e "${CYAN}[PROJECT]${NC} $1"
}

# Check and setup ESP-IDF environment automatically
check_environment() {
    print_status "Checking build environment..."
    
    if [ ! -d "$ESP_IDF_PATH" ]; then
        print_error "ESP-IDF not found at $ESP_IDF_PATH"
        print_error "Please ensure ESP-IDF is properly installed"
        exit 1
    fi
    
    # Always source ESP-IDF to ensure clean environment
    if [ -f "$ESP_IDF_PATH/export.sh" ]; then
        print_status "Sourcing ESP-IDF environment..."
        # Source in a way that preserves the environment for this script
        set +e  # Don't exit on sourcing errors
        source "$ESP_IDF_PATH/export.sh" > /dev/null 2>&1
        local source_result=$?
        set -e  # Re-enable exit on error
        
        if [ $source_result -ne 0 ]; then
            print_warning "ESP-IDF sourcing had warnings (this is often normal)"
        fi
    else
        print_error "ESP-IDF export script not found at $ESP_IDF_PATH/export.sh"
        exit 1
    fi
    
    # Verify ESP-IDF is properly sourced
    if [ -z "$IDF_PATH" ]; then
        print_error "ESP-IDF environment not properly initialized"
        print_error "Try running: source $ESP_IDF_PATH/export.sh manually first"
        exit 1
    fi
    
    # Verify idf.py is available
    if ! command -v idf.py &> /dev/null; then
        print_error "idf.py command not found in PATH"
        print_error "ESP-IDF environment may not be properly set up"
        exit 1
    fi
    
    print_status "ESP-IDF environment ready: $IDF_PATH"
    print_status "ESP-IDF version: $(idf.py --version 2>/dev/null | head -n1 || echo 'Unknown')"
}

# List all discovered projects
list_projects() {
    discover_projects
    
    print_header
    echo -e "${BOLD}Discovered Projects:${NC}"
    echo
    
    for project in "${!PROJECTS[@]}"; do
        local path="${PROJECTS[$project]}"
        if [ -z "$path" ]; then
            print_error "Project '$project' has no path defined!"
            continue
        fi

        local desc="${PROJECT_DESCRIPTIONS[$project]}"
        local target="${PROJECT_TARGETS[$project]}"

        if [ -d "$path" ]; then
            echo -e "${GREEN}✓${NC} ${BOLD}$project${NC} (${target})"
        else
            echo -e "${RED}✗${NC} ${BOLD}$project${NC} (${target}) - ${RED}Missing${NC}"
        fi
        echo -e "  ${CYAN}→${NC} $desc"
        echo -e "  ${YELLOW}Path:${NC} $path"
        if [ -n "${PROJECT_FEATURES[$project]}" ]; then
            echo -e "  ${BLUE}Features:${NC} ${PROJECT_FEATURES[$project]}"
        fi
        echo
    done
}

# Create a new project from template
create_project() {
    local project_name="$1"
    
    if [ -z "$project_name" ]; then
        print_error "Usage: $0 create <project_name>"
        echo "Example: $0 create my_camera"
        exit 1
    fi
    
    local full_path="$ESP_PROJECTS_PATH/$project_name"
    
    if [ -d "$full_path" ]; then
        print_error "Project directory already exists: $full_path"
        exit 1
    fi
    
    print_status "Creating new project: $project_name"
    print_status "Location: $full_path"
    
    # Create project structure
    mkdir -p "$full_path"/{main,components,tools,docs,tests}
    
    # Copy CMakeLists.txt template
    cp "$full_path/CMakeLists.txt" "$full_path/"
    
    # Create basic main.c
    cat > "$full_path/main/main.c" << EOF
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "$project_name";

void app_main(void) {
    ESP_LOGI(TAG, "=== $project_name Starting ===");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "System initialized successfully!");
    
    while (1) {
        ESP_LOGI(TAG, "Running...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
EOF
    
    # Create main CMakeLists.txt
    cat > "$full_path/main/CMakeLists.txt" << EOF
idf_component_register(
    SRCS 
        "main.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        nvs_flash
)
EOF
    
    # Create build script
    cat > "$full_path/tools/build.sh" << EOF
#!/bin/bash
# Auto-generated build script for $project_name
exec "$REPO_ROOT/build.sh" build-project "$project_name" "\$@"
EOF
    chmod +x "$full_path/tools/build.sh"
    
    # Create README
    cat > "$full_path/README.md" << EOF
# $project_name

Project description here.

## Build Instructions

\`\`\`bash
# From repository root
./build.sh build $project_name

# Or from project directory
./tools/build.sh build
\`\`\`
EOF
    
    print_status "Project created successfully!"
    print_status "Add to registry by editing PROJECTS array in build.sh"
}

# Helper function to detect and build UI if present
build_ui_if_present() {
    local project_path="$1"
    
    # Check if project has a UI directory with build.js
    if [ -d "$project_path/ui" ] && [ -f "$project_path/ui/build.js" ]; then
        print_status "UI detected - building web interface..."
        
        cd "$project_path/ui"
        
        # Install dependencies if node_modules doesn't exist
        if [ ! -d "node_modules" ]; then
            print_status "Installing UI dependencies..."
            npm install
        fi
        
        # Build the UI
        node build.js
        if [ $? -eq 0 ]; then
            print_status "UI built successfully"
        else
            print_error "UI build failed"
            return 1
        fi
        
        cd "$project_path"
    fi
    
    return 0
}

# Helper function to clean UI artifacts
clean_ui_artifacts() {
    local project_path="$1"
    
    # Remove generated UI header file
    if [ -f "$project_path/components/web_ui_module/include/camera_web_ui.h" ]; then
        rm -f "$project_path/components/web_ui_module/include/camera_web_ui.h"
        print_status "Removed generated UI header file"
    fi
    
    # Remove UI build artifacts
    if [ -d "$project_path/ui/dist" ]; then
        rm -rf "$project_path/ui/dist"
        print_status "Removed UI dist directory"
    fi
}

# Build a specific project
build_project() {
    local project_name="$1"
    local command="$2"
    
    if [ -z "$project_name" ]; then
        print_error "Usage: $0 build <project_name> [command]"
        echo "Run '$0 list' to see available projects"
        exit 1
    fi
    
    # Discover projects first
    discover_projects
    
    if [ -z "${PROJECTS[$project_name]}" ]; then
        print_error "Unknown project: $project_name"
        echo "Available projects: ${!PROJECTS[*]}"
        echo "Run '$0 list' to see all discovered projects"
        exit 1
    fi
    
    local project_path="${PROJECTS[$project_name]}"
    local target="${PROJECT_TARGETS[$project_name]}"
    
    if [ ! -d "$project_path" ]; then
        print_error "Project directory not found: $project_path"
        exit 1
    fi
    
    print_project "Building project: $project_name"
    print_status "Path: $project_path"
    print_status "Target: $target"
    
    cd "$project_path"
    
    case "${command:-build}" in
        setup)
            print_status "Setting up project target: $target"
            idf.py set-target "$target"
            ;;
        build)
            print_status "Building project..."
            
            # Build UI first if present
            build_ui_if_present "$project_path"
            if [ $? -ne 0 ]; then
                print_error "UI build failed - aborting"
                exit 1
            fi
            
            idf.py build
            ;;
        flash)
            print_status "Flashing project..."
            idf.py flash
            ;;
        monitor)
            print_status "Starting monitor..."
            idf.py monitor
            ;;
        all)
            print_status "Building and flashing..."
            
            # Build UI first if present
            build_ui_if_present "$project_path"
            if [ $? -ne 0 ]; then
                print_error "UI build failed - aborting"
                exit 1
            fi
            
            idf.py build flash
            ;;
        clean)
            print_status "Cleaning project..."
            idf.py clean
            ;;
        fullclean)
            print_status "Full clean (including sdkconfig)..."
            
            # Clean UI artifacts first
            clean_ui_artifacts "$project_path"
            
            idf.py fullclean
            ;;
        menuconfig)
            print_status "Opening ESP-IDF configuration menu..."
            idf.py menuconfig
            ;;
        saveconfig)
            print_status "Saving current configuration as defaults..."
            idf.py save-defconfig
            ;;
        size)
            print_status "Analyzing build size..."
            idf.py size
            ;;
        size-components)
            print_status "Analyzing component sizes..."
            idf.py size-components
            ;;
        size-files)
            print_status "Analyzing file sizes..."
            idf.py size-files
            ;;
        erase-flash)
            print_status "Erasing flash..."
            idf.py erase-flash
            ;;
        app-flash)
            print_status "Flashing app only..."
            idf.py app-flash
            ;;
        bootloader-flash)
            print_status "Flashing bootloader..."
            idf.py bootloader-flash
            ;;
        partition-table-flash)
            print_status "Flashing partition table..."
            idf.py partition-table-flash
            ;;
        encrypted-app-flash)
            print_status "Flashing encrypted app..."
            idf.py encrypted-app-flash
            ;;
        encrypted-flash)
            print_status "Flashing encrypted firmware..."
            idf.py encrypted-flash
            ;;
        monitor-dfu)
            print_status "Starting DFU monitor..."
            idf.py dfu
            ;;
        uf2)
            print_status "Creating UF2 file..."
            idf.py uf2
            ;;
        uf2-app)
            print_status "Creating UF2 app file..."
            idf.py uf2-app
            ;;
        *)
            # Check if it's a direct idf.py command passthrough
            if [[ "$command" =~ ^idf\. ]]; then
                local idf_cmd="${command#idf.}"
                print_status "Executing idf.py $idf_cmd..."
                shift 2  # Remove project_name and command
                idf.py "$idf_cmd" "$@"
            else
                print_error "Unknown command: $command"
                echo
                echo "🔧 Common Commands:"
                echo "  setup, build, flash, monitor, all, clean, fullclean"
                echo
                echo "📊 Analysis Commands:"
                echo "  size, size-components, size-files"
                echo
                echo "⚙️  Configuration Commands:"
                echo "  menuconfig, saveconfig"
                echo
                echo "🔥 Flash Commands:"
                echo "  erase-flash, app-flash, bootloader-flash, partition-table-flash"
                echo "  encrypted-app-flash, encrypted-flash"
                echo
                echo "🔌 Advanced Commands:"
                echo "  monitor-dfu, uf2, uf2-app"
                echo
                echo "🚀 Direct idf.py Passthrough:"
                echo "  Use 'idf.<command>' to pass any command directly to idf.py"
                echo "  Example: $0 build $project_name idf.gdb"
                exit 1
            fi
            ;;
    esac
}

# Configuration management helper
config_project() {
    local project_name="$1"
    
    if [ -z "$project_name" ]; then
        print_error "Usage: $0 config <project_name>"
        echo "This opens the ESP-IDF configuration menu for the specified project"
        exit 1
    fi
    
    discover_projects
    
    if [ -z "${PROJECTS[$project_name]}" ]; then
        print_error "Unknown project: $project_name"
        echo "Available projects: ${!PROJECTS[*]}"
        exit 1
    fi
    
    local project_path="${PROJECTS[$project_name]}"
    
    print_project "Configuring project: $project_name"
    print_status "Path: $project_path"
    
    cd "$project_path"
    
    echo -e "${BOLD}Configuration Options:${NC}"
    echo "1. menuconfig  - Full configuration menu"
    echo "2. saveconfig  - Save current config as defaults"
    echo "3. size        - Analyze current build size"
    echo "4. show        - Show current target and config"
    echo
    read -p "Select option (1-4) or press Enter for menuconfig: " choice
    
    case "${choice:-1}" in
        1)
            print_status "Opening configuration menu..."
            idf.py menuconfig
            ;;
        2)
            print_status "Saving configuration as defaults..."
            idf.py save-defconfig
            ;;
        3)
            if [ -d "build" ]; then
                print_status "Analyzing build size..."
                idf.py size
            else
                print_warning "No build found. Building first..."
                idf.py build
                idf.py size
            fi
            ;;
        4)
            print_status "Current configuration:"
            if [ -f "sdkconfig" ]; then
                echo "Target: $(grep 'CONFIG_IDF_TARGET=' sdkconfig | cut -d'=' -f2 | tr -d '"')"
                echo "Config file: sdkconfig"
                if [ -f "sdkconfig.defaults" ]; then
                    echo "Defaults file: sdkconfig.defaults"
                fi
            else
                echo "No configuration found. Run setup first."
            fi
            ;;
        *)
            print_error "Invalid option: $choice"
            ;;
    esac
}

# Initialize repository
init_repo() {
    print_header
    print_status "Initializing Hummingbird repository..."
    
    check_environment
    
    # Initialize git submodules if needed
    if [ -f "$REPO_ROOT/.gitmodules" ]; then
        print_status "Updating git submodules..."
        cd "$REPO_ROOT"
        git submodule update --init --recursive
    fi
    
    print_status "Repository initialization complete!"
}

# Show help
show_help() {
    print_header
    echo -e "${BOLD}Usage:${NC} $0 [command] [options]"
    echo
    echo -e "${BOLD}🚀 Quick Start:${NC}"
    echo "  $0 list                              # See all projects"
    echo "  $0 build timelapse all               # Build and flash main project"
    echo
    echo -e "${BOLD}Commands:${NC}"
    echo "  ${GREEN}list${NC}                    - List all discovered projects"
    echo "  ${GREEN}init${NC}                    - Initialize repository and environment"
    echo "  ${GREEN}create${NC} <name>           - Create new project from template"
    echo "  ${GREEN}build${NC} <project> [cmd]   - Build specific project"
    echo "  ${GREEN}config${NC} <project>        - Interactive project configuration"
    echo "  ${GREEN}help${NC}                    - Show this help"
    echo
    echo -e "${BOLD}🔧 Common Build Commands:${NC}"
    echo "  setup           - Configure project target"
    echo "  build           - Build project"
    echo "  flash           - Flash to device"
    echo "  monitor         - Start serial monitor"
    echo "  all             - Build and flash"
    echo "  clean           - Clean build files"
    echo "  fullclean       - Clean all generated files"
    echo
    echo -e "${BOLD}⚙️  Configuration & Analysis:${NC}"
    echo "  menuconfig      - Open ESP-IDF configuration menu"
    echo "  saveconfig      - Save current config as defaults"
    echo "  size            - Analyze build size"
    echo "  size-components - Analyze component sizes"
    echo "  size-files      - Analyze file sizes"
    echo
    echo -e "${BOLD}🔥 Advanced Flash Commands:${NC}"
    echo "  erase-flash           - Erase entire flash"
    echo "  app-flash             - Flash application only"
    echo "  bootloader-flash      - Flash bootloader only"
    echo "  partition-table-flash - Flash partition table only"
    echo "  encrypted-app-flash   - Flash encrypted application"
    echo "  encrypted-flash       - Flash encrypted firmware"
    echo
    echo -e "${BOLD}🚀 Advanced Tools:${NC}"
    echo "  monitor-dfu     - DFU monitor mode"
    echo "  uf2             - Create UF2 file"
    echo "  uf2-app         - Create UF2 app file"
    echo "  idf.<command>   - Direct idf.py passthrough"
    echo
    echo -e "${BOLD}Examples:${NC}"
    echo "  $0 list"
    echo "  $0 build timelapse setup"
    echo "  $0 build timelapse all"
    echo "  $0 create my_project projects/my_project"
    echo
    echo -e "${BOLD}Project-specific shortcuts:${NC}"
    echo "  cd timelapse && ./tools/build.sh build"
    echo
    echo -e "${BOLD}💡 Note:${NC} This script automatically sources ESP-IDF - no manual setup required!"
}

# Main execution
main() {
    case "${1:-help}" in
        list|ls)
            list_projects
            ;;
        init)
            init_repo
            ;;
        create)
            create_project "$2" "$3"
            ;;
        build)
            check_environment
            build_project "$2" "$3"
            ;;
        build-project)
            # Internal command used by project build scripts
            check_environment
            build_project "$2" "$3"
            ;;
        config)
            check_environment
            config_project "$2"
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            print_error "Unknown command: $1"
            show_help
            exit 1
            ;;
    esac
}

main "$@"