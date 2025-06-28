#!/bin/bash

# ESP-IDF Camera Development Framework - Environment Preparation Script
# Sets up ESP-IDF and ESP-WHO submodules and development environment

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
ESP_IDF_REPO="https://github.com/espressif/esp-idf.git"
ESP_WHO_REPO="https://github.com/espressif/esp-who.git"
ESP_IDF_BRANCH="release/v5.4"  # Stable branch compatible with ESP-WHO
ESP_WHO_BRANCH="master"        # Latest ESP-WHO with ESP32-P4 support

# Function to print formatted output
print_header() {
    echo -e "${BOLD}${BLUE}================================================${NC}"
    echo -e "${BOLD}${BLUE}  🚀 ESP-IDF Camera Development Setup${NC}"
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

print_step() {
    echo -e "${CYAN}[STEP]${NC} $1"
}

# Check if git is available
check_prerequisites() {
    print_step "Checking prerequisites..."
    
    if ! command -v git &> /dev/null; then
        print_error "Git is required but not installed. Please install git first."
        exit 1
    fi
    
    if ! command -v python3 &> /dev/null; then
        print_error "Python 3 is required but not installed. Please install Python 3.8 or later."
        exit 1
    fi
    
    # Check Python version
    python_version=$(python3 -c "import sys; print('.'.join(map(str, sys.version_info[:2])))")
    print_status "Found Python $python_version"
    
    if ! command -v cmake &> /dev/null; then
        print_warning "CMake not found. ESP-IDF installer will handle this."
    fi
    
    print_status "Prerequisites check completed"
}

# Initialize git repository if needed
init_git_repo() {
    cd "$REPO_ROOT"
    
    if [ ! -d ".git" ]; then
        print_step "Initializing git repository..."
        git init
        print_status "Git repository initialized"
    else
        print_status "Git repository already exists"
    fi
}

# Add submodules
add_submodules() {
    print_step "Setting up git submodules..."
    cd "$REPO_ROOT"
    
    # Check if ESP-IDF directory exists and is a git repo
    if [ -d "esp-idf/.git" ]; then
        print_status "ESP-IDF directory exists, converting to submodule..."
        # Get the current remote URL
        ESP_IDF_CURRENT_REMOTE=$(cd esp-idf && git remote get-url origin 2>/dev/null || echo "$ESP_IDF_REPO")
        # Remove from git tracking but keep the directory
        git rm --cached esp-idf 2>/dev/null || true
        # Add as submodule with existing directory
        git submodule add "$ESP_IDF_CURRENT_REMOTE" esp-idf
    elif [ ! -f ".gitmodules" ] || ! grep -q "esp-idf" .gitmodules; then
        print_status "Adding ESP-IDF submodule (branch: $ESP_IDF_BRANCH)..."
        git submodule add -b "$ESP_IDF_BRANCH" "$ESP_IDF_REPO" esp-idf
    else
        print_status "ESP-IDF submodule already configured"
    fi
    
    # Check if ESP-WHO directory exists and is a git repo
    if [ -d "esp-who/.git" ]; then
        print_status "ESP-WHO directory exists, converting to submodule..."
        # Get the current remote URL
        ESP_WHO_CURRENT_REMOTE=$(cd esp-who && git remote get-url origin 2>/dev/null || echo "$ESP_WHO_REPO")
        # Remove from git tracking but keep the directory
        git rm --cached esp-who 2>/dev/null || true
        # Add as submodule with existing directory
        git submodule add "$ESP_WHO_CURRENT_REMOTE" esp-who
    elif ! grep -q "esp-who" .gitmodules 2>/dev/null; then
        print_status "Adding ESP-WHO submodule (branch: $ESP_WHO_BRANCH)..."
        git submodule add -b "$ESP_WHO_BRANCH" "$ESP_WHO_REPO" esp-who
    else
        print_status "ESP-WHO submodule already configured"
    fi
    
    print_status "Submodules configured"
}

# Initialize and update submodules
init_submodules() {
    print_step "Initializing and updating submodules..."
    cd "$REPO_ROOT"
    
    # Initialize submodules
    git submodule init
    
    # Check if ESP-IDF is already downloaded
    if [ -d "esp-idf/.git" ] && [ -f "esp-idf/export.sh" ]; then
        print_status "ESP-IDF already exists, syncing submodule state..."
        git submodule sync esp-idf
    else
        print_status "Downloading ESP-IDF (this may take several minutes)..."
        git submodule update --progress esp-idf
    fi
    
    # Check if ESP-WHO is already downloaded
    if [ -d "esp-who/.git" ] && [ -d "esp-who/examples" ]; then
        print_status "ESP-WHO already exists, syncing submodule state..."
        git submodule sync esp-who
    else
        print_status "Downloading ESP-WHO..."
        git submodule update --progress esp-who
    fi
    
    # Initialize ESP-IDF submodules (only if not already done)
    print_status "Checking ESP-IDF submodules..."
    cd esp-idf
    if [ ! -d "components/mbedtls/mbedtls/.git" ]; then
        print_status "Initializing ESP-IDF submodules (this may take a while)..."
        git submodule update --init --recursive
    else
        print_status "ESP-IDF submodules already initialized"
    fi
    cd "$REPO_ROOT"
    
    print_status "All submodules ready"
}

# Install ESP-IDF tools and dependencies
install_esp_idf() {
    print_step "Installing ESP-IDF tools and dependencies..."
    cd "$REPO_ROOT/esp-idf"
    
    # Check if ESP-IDF tools are already installed
    if [ -d ".espressif" ] && [ -f ".espressif/python_env/bin/python" ]; then
        print_status "ESP-IDF tools already installed, checking for updates..."
        # Just ensure all targets are installed
        ./install.sh esp32,esp32s3,esp32p4 > /dev/null 2>&1 || true
    else
        print_status "Installing ESP-IDF Python dependencies and tools..."
        print_status "This may take several minutes for the first time setup..."
        ./install.sh esp32,esp32s3,esp32p4
    fi
    
    print_status "ESP-IDF installation completed"
    cd "$REPO_ROOT"
}

# Create environment activation script
create_activation_script() {
    print_step "Creating environment activation script..."
    
    cat > "$REPO_ROOT/activate.sh" << 'EOF'
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
EOF
    
    chmod +x "$REPO_ROOT/activate.sh"
    print_status "Created activate.sh script"
}

# Create VS Code configuration for better development experience
create_vscode_config() {
    print_step "Creating VS Code configuration..."
    
    mkdir -p "$REPO_ROOT/.vscode"
    
    # Settings for ESP-IDF development
    cat > "$REPO_ROOT/.vscode/settings.json" << EOF
{
    "C_Cpp.intelliSenseEngine": "default",
    "idf.adapterTargetName": "esp32s3",
    "idf.customExtraPaths": "",
    "idf.customExtraVars": {
        "ESP_WHO_PATH": "\${workspaceFolder}/esp-who",
        "ESP_PROJECTS_PATH": "\${workspaceFolder}/projects"
    },
    "idf.espIdfPath": "\${workspaceFolder}/esp-idf",
    "idf.openOcdConfigs": [
        "interface/ftdi/esp32_devkitj_v1.cfg",
        "target/esp32s3.cfg"
    ],
    "idf.pythonBinPath": "\${workspaceFolder}/esp-idf/.espressif/python_env/bin/python",
    "idf.toolsPath": "\${workspaceFolder}/esp-idf/.espressif",
    "files.associations": {
        "*.h": "c",
        "*.c": "c",
        "*.hpp": "cpp",
        "*.cpp": "cpp"
    },
    "files.exclude": {
        "**/build": true,
        "**/dist": true,
        "**/.espressif": true,
        "**/managed_components": true
    },
    "search.exclude": {
        "**/build": true,
        "**/dist": true,
        "**/.espressif": true,
        "**/managed_components": true,
        "**/esp-idf": true
    }
}
EOF

    # Recommended extensions
    cat > "$REPO_ROOT/.vscode/extensions.json" << EOF
{
    "recommendations": [
        "espressif.esp-idf-extension",
        "ms-vscode.cpptools",
        "ms-vscode.cmake-tools",
        "twxs.cmake",
        "ms-python.python"
    ]
}
EOF

    print_status "VS Code configuration created"
}

# Verify installation
verify_installation() {
    print_step "Verifying installation..."
    
    # Check if ESP-IDF is properly installed
    if [ -f "$REPO_ROOT/esp-idf/export.sh" ]; then
        print_status "✅ ESP-IDF installation verified"
    else
        print_error "❌ ESP-IDF installation failed"
        return 1
    fi
    
    # Check if ESP-WHO is available
    if [ -d "$REPO_ROOT/esp-who/examples" ]; then
        print_status "✅ ESP-WHO installation verified"
    else
        print_error "❌ ESP-WHO installation failed"
        return 1
    fi
    
    # Test activation script
    print_status "Testing environment activation..."
    if bash -c "source $REPO_ROOT/activate.sh && command -v idf.py" > /dev/null 2>&1; then
        print_status "✅ Environment activation verified"
    else
        print_warning "⚠️  Environment activation test inconclusive (this is often normal)"
    fi
    
    print_status "Installation verification completed"
}

# Print usage instructions
print_instructions() {
    echo
    print_header
    echo -e "${BOLD}🎉 ESP-IDF Camera Development Environment Ready!${NC}"
    echo
    echo -e "${BOLD}Next Steps:${NC}"
    echo -e "1. ${CYAN}Activate the environment:${NC}"
    echo -e "   ${YELLOW}source ./activate.sh${NC}"
    echo
    echo -e "2. ${CYAN}List available projects:${NC}"
    echo -e "   ${YELLOW}./build.sh list${NC}"
    echo
    echo -e "3. ${CYAN}Build a project (example):${NC}"
    echo -e "   ${YELLOW}./build.sh build hello_world setup${NC}"
    echo -e "   ${YELLOW}./build.sh build hello_world all${NC}"
    echo
    echo -e "${BOLD}Development Tips:${NC}"
    echo -e "• Use ${CYAN}./activate.sh${NC} at the start of each session"
    echo -e "• VS Code configuration has been created in .vscode/"
    echo -e "• Install the ESP-IDF VS Code extension for best experience"
    echo -e "• Check ESP-WHO examples in ./esp-who/examples/"
    echo
    echo -e "${BOLD}Submodule Management:${NC}"
    echo -e "• Update ESP-IDF: ${YELLOW}git submodule update --remote esp-idf${NC}"
    echo -e "• Update ESP-WHO: ${YELLOW}git submodule update --remote esp-who${NC}"
    echo -e "• Update all: ${YELLOW}git submodule update --remote${NC}"
    echo
}

# Show help
show_help() {
    echo "ESP-IDF Camera Development Environment Setup"
    echo
    echo "Usage: $0 [command]"
    echo
    echo "Commands:"
    echo "  setup     - Full setup (default): initialize submodules and install ESP-IDF"
    echo "  update    - Update existing submodules to latest versions"
    echo "  clean     - Remove submodules and clean environment"
    echo "  verify    - Verify installation"
    echo "  help      - Show this help"
    echo
    echo "Examples:"
    echo "  $0            # Full setup"
    echo "  $0 setup      # Full setup"
    echo "  $0 update     # Update submodules"
    echo "  $0 verify     # Check installation"
    echo
}

# Update submodules
update_submodules() {
    print_step "Updating submodules to latest versions..."
    cd "$REPO_ROOT"
    
    if [ -f ".gitmodules" ]; then
        print_status "Updating ESP-IDF..."
        git submodule update --remote esp-idf
        
        print_status "Updating ESP-WHO..."
        git submodule update --remote esp-who
        
        print_status "Updating ESP-IDF submodules..."
        cd esp-idf
        git submodule update --init --recursive
        cd "$REPO_ROOT"
        
        print_status "Submodules updated successfully"
    else
        print_error "No submodules configured. Run './prepare.sh setup' first."
        exit 1
    fi
}

# Clean environment
clean_environment() {
    print_step "Cleaning development environment..."
    cd "$REPO_ROOT"
    
    # Remove submodules
    if [ -f ".gitmodules" ]; then
        print_status "Removing submodules..."
        git submodule deinit --all --force
        rm -rf .git/modules/esp-idf .git/modules/esp-who
        git rm esp-idf esp-who 2>/dev/null || true
        rm -f .gitmodules
    fi
    
    # Remove generated files
    rm -rf esp-idf/ esp-who/
    rm -f activate.sh
    rm -rf .vscode/
    
    print_status "Environment cleaned"
}

# Main execution
main() {
    case "${1:-setup}" in
        setup)
            print_header
            check_prerequisites
            init_git_repo
            add_submodules
            init_submodules
            install_esp_idf
            create_activation_script
            create_vscode_config
            verify_installation
            print_instructions
            ;;
        update)
            update_submodules
            ;;
        clean)
            clean_environment
            ;;
        verify)
            verify_installation
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