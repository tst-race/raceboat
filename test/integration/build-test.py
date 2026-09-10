#!/usr/bin/env python3
"""
build-test.py
Modular build-and-test orchestrator for raceboat plugins.

This script provides fine-grained control over the rebuild-test workflow,
allowing developers to rebuild only what's necessary before running integration tests.

Build Pipeline:
  1. Raceboat Code     - Compile raceboat SDK
  2. Plugin Builder    - Docker image for plugin compilation
  3. Runtime Image     - Docker image with race-cli for tests
  4. Plugin Build      - Compile specific plugin
  5. Integration Test  - Run docker-compose test

Examples:
  # Test without rebuilding (relative path)
  python3 build-test.py --plugin-dir ../../racebird
  
  # Rebuild plugin only, then test (absolute path)
  python3 build-test.py --plugin-dir /path/to/my-plugin --rebuild-plugin
  
  # Rebuild raceboat, then test
  python3 build-test.py --plugin-dir ../../racebird --rebuild-raceboat
  
  # Rebuild everything
  python3 build-test.py --plugin-dir ../../racebird --rebuild-all
  
  # Granular control
  python3 build-test.py --plugin-dir ../../racebird --rebuild-raceboat-code --rebuild-plugin
"""

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List, Optional

# ANSI colors
RED = '\033[0;31m'
GREEN = '\033[0;32m'
YELLOW = '\033[1;33m'
BLUE = '\033[0;34m'
CYAN = '\033[0;36m'
MAGENTA = '\033[0;35m'
NC = '\033[0m'


def colored(text: str, color: str) -> str:
    """Return colored text."""
    return f"{color}{text}{NC}"


class BuildStage:
    """Represents a single build stage with its command and metadata."""
    
    def __init__(self, name: str, description: str, command: List[str], 
                 cwd: Path, shell: bool = False):
        self.name = name
        self.description = description
        self.command = command
        self.cwd = cwd
        self.shell = shell
    
    def execute(self, dry_run: bool = False, verbose: bool = False) -> bool:
        """Execute this build stage."""
        print(f"\n{colored('▶', CYAN)} {colored(self.description, BLUE)}")
        
        if verbose:
            cmd_str = ' '.join(self.command) if isinstance(self.command, list) else self.command
            print(f"  {colored('CWD:', YELLOW)} {self.cwd}")
            print(f"  {colored('CMD:', YELLOW)} {cmd_str}")
        
        if dry_run:
            print(f"  {colored('[DRY-RUN]', MAGENTA)} Would execute command")
            return True
        
        try:
            if self.shell:
                result = subprocess.run(
                    self.command,
                    cwd=self.cwd,
                    shell=True,
                    check=True
                )
            else:
                result = subprocess.run(
                    self.command,
                    cwd=self.cwd,
                    check=True
                )
            print(f"  {colored('✓', GREEN)} {self.name} completed")
            return True
        except subprocess.CalledProcessError as e:
            print(f"  {colored('✗', RED)} {self.name} failed with exit code {e.returncode}")
            return False


class BuildTestOrchestrator:
    """Orchestrates modular build and test workflow."""
    
    def __init__(self, raceboat_root: Path, plugin_dir: Path, verbose: bool = False):
        self.raceboat_root = raceboat_root
        self.plugin_dir = plugin_dir
        self.plugin_name = plugin_dir.name
        self.verbose = verbose
        
        # Validate plugin directory structure
        self._validate_plugin_structure()
    
    def _validate_plugin_structure(self):
        """Validate that plugin directory has required structure."""
        if not self.plugin_dir.exists():
            raise ValueError(f"Plugin directory does not exist: {self.plugin_dir}")
        
        if not self.plugin_dir.is_dir():
            raise ValueError(f"Plugin path is not a directory: {self.plugin_dir}")
        
        # Check for build script
        build_script = self.plugin_dir / 'build_artifacts_in_docker_image.sh'
        if not build_script.exists():
            raise ValueError(
                f"Plugin directory missing required build script: build_artifacts_in_docker_image.sh\n"
                f"Expected at: {build_script}"
            )
        
        # Check for test directory structure
        test_dir = self.plugin_dir / 'test'
        if not test_dir.exists():
            raise ValueError(f"Plugin directory missing test/ subdirectory: {test_dir}")
        
        setup_script = test_dir / 'setup.py'
        if not setup_script.exists():
            raise ValueError(
                f"Plugin test directory missing setup.py\n"
                f"Expected at: {setup_script}"
            )
        
        compose_file = test_dir / 'docker-compose.yml'
        if not compose_file.exists():
            raise ValueError(
                f"Plugin test directory missing docker-compose.yml\n"
                f"Expected at: {compose_file}"
            )
    
    def print_header(self, title: str):
        """Print a formatted header."""
        border = '=' * 60
        print(f"\n{colored(border, BLUE)}")
        print(f"{colored(title, BLUE)}")
        print(f"{colored(border, BLUE)}")
    
    def build_raceboat_code(self, dry_run: bool = False) -> bool:
        """Stage 1: Compile raceboat code in Docker."""
        stage = BuildStage(
            name='raceboat-code',
            description='Building raceboat code (Stage 1/3)',
            command=[
                'docker', 'run', '-it', '--rm', '--name=build-raceboat',
                '-e', 'MAKEFLAGS=-j',
                '-v', f'{self.raceboat_root}:/code/',
                '-w', '/code',
                'ghcr.io/tst-race/raceboat/raceboat-builder:latest',
                './build.sh'
            ],
            cwd=self.raceboat_root
        )
        return stage.execute(dry_run, self.verbose)
    
    @staticmethod
    def detect_host_architecture() -> str:
        """Return the Docker target architecture for the current host."""
        machine = platform.machine().lower()
        if machine in {'x86_64', 'amd64'}:
            return '--platform-x86_64'
        if machine in {'arm64', 'aarch64'}:
            return '--platform-arm64'
        raise ValueError(
            f'Unsupported host architecture for Docker builds: {platform.machine()}. '
            'Expected x86_64 or arm64/aarch64.'
        )

    def build_plugin_builder_image(self, dry_run: bool = False) -> bool:
        """Stage 2: Build plugin builder Docker image."""
        stage = BuildStage(
            name='plugin-builder-image',
            description='Building plugin builder image (Stage 2/3)',
            command=[
                './build_image.sh',
                '-n', 'ghcr.io/tst-race/raceboat',
                self.detect_host_architecture()
            ],
            cwd=self.raceboat_root / 'raceboat-plugin-builder-image'
        )
        return stage.execute(dry_run, self.verbose)
    
    def build_runtime_image(self, dry_run: bool = False) -> bool:
        """Stage 3: Build runtime Docker image."""
        stage = BuildStage(
            name='runtime-image',
            description='Building runtime image (Stage 3/3)',
            command=[
                './build_image.sh',
                '-n', 'ghcr.io/tst-race/raceboat',
                self.detect_host_architecture()
            ],
            cwd=self.raceboat_root / 'raceboat-runtime-image'
        )
        return stage.execute(dry_run, self.verbose)
    
    def build_plugin(self, dry_run: bool = False) -> bool:
        """Build the plugin."""
        # Build plugin artifacts
        build_stage = BuildStage(
            name=f'{self.plugin_name}-build',
            description=f'Building {self.plugin_name} plugin',
            command=['./build_artifacts_in_docker_image.sh'],
            cwd=self.plugin_dir
        )
        
        if not build_stage.execute(dry_run, self.verbose):
            return False
        
        # Run setup to copy artifacts
        setup_stage = BuildStage(
            name=f'{self.plugin_name}-setup',
            description=f'Setting up {self.plugin_name} test artifacts',
            command=[sys.executable, 'setup.py'],
            cwd=self.plugin_dir / 'test'
        )
        
        return setup_stage.execute(dry_run, self.verbose)
    
    def clear_plugin_logs(self, dry_run: bool = False) -> bool:
        """Clear plugin test logs."""
        test_dir = self.plugin_dir / 'test'
        log_dirs = ['server-logs', 'client-logs', 'client2-logs']
        
        print(f"\n{colored('▶', CYAN)} {colored('Clearing test logs', BLUE)}")
        
        if dry_run:
            print(f"  {colored('[DRY-RUN]', MAGENTA)} Would clear log directories")
            return True
        
        for log_dir_name in log_dirs:
            log_dir = test_dir / log_dir_name
            if log_dir.exists():
                for item in log_dir.iterdir():
                    try:
                        if item.is_file():
                            item.unlink()
                        elif item.is_dir():
                            shutil.rmtree(item)
                    except Exception as e:
                        if self.verbose:
                            print(f"  {colored('Warning:', YELLOW)} Could not delete {item}: {e}")
        
        print(f"  {colored('✓', GREEN)} Logs cleared")
        return True
    
    def run_integration_test(self, wait_time: int = 10, dry_run: bool = False) -> bool:
        """Run integration test for plugin."""
        compose_file = self.plugin_dir / 'test' / 'docker-compose.yml'
        
        test_runner = Path(__file__).parent / 'run-integration-test.py'
        
        stage = BuildStage(
            name=f'{self.plugin_name}-test',
            description=f'Running {self.plugin_name} integration test',
            command=[
                sys.executable,
                str(test_runner),
                '--compose-file', str(compose_file),
                '--wait-time', str(wait_time),
                '--name', f'{self.plugin_name} Plugin Integration Test',
                '--clear-logs'
            ],
            cwd=Path(__file__).parent
        )
        
        return stage.execute(dry_run, self.verbose)
    
    def orchestrate(self, config: dict) -> int:
        """Main orchestration logic."""
        dry_run = config['dry_run']
        
        self.print_header(f'Build & Test: {self.plugin_name}')
        
        # Determine what to build
        build_raceboat_code = (
            config['rebuild_all'] or
            config['rebuild_raceboat'] or
            config['rebuild_raceboat_code']
        )
        
        build_plugin_builder = (
            config['rebuild_all'] or
            config['rebuild_raceboat'] or
            config['rebuild_raceboat_images'] or
            config['rebuild_plugin_builder']
        )
        
        build_runtime = (
            config['rebuild_all'] or
            config['rebuild_raceboat'] or
            config['rebuild_raceboat_images'] or
            config['rebuild_runtime_image']
        )
        
        build_plugin = (
            config['rebuild_all'] or
            config['rebuild_plugin']
        )
        
        skip_build = config['skip_build']
        skip_test = config['skip_test']
        
        # Validate combinations
        if skip_build and (build_raceboat_code or build_plugin_builder or 
                          build_runtime or build_plugin):
            print(f"{colored('Error:', RED)} Cannot use --skip-build with rebuild options")
            return 1
        
        # Print plan
        if not skip_build:
            print(f"\n{colored('Build Plan:', CYAN)}")
            if build_raceboat_code:
                print(f"  • {colored('Rebuild raceboat code', YELLOW)}")
            if build_plugin_builder:
                print(f"  • {colored('Rebuild plugin builder image', YELLOW)}")
            if build_runtime:
                print(f"  • {colored('Rebuild runtime image', YELLOW)}")
            if build_plugin:
                print(f"  • {colored(f'Rebuild {self.plugin_name} plugin', YELLOW)}")
            if not (build_raceboat_code or build_plugin_builder or build_runtime or build_plugin):
                print(f"  • {colored('No rebuilds requested', GREEN)}")
        else:
            print(f"\n{colored('Build Plan:', CYAN)}")
            print(f"  • {colored('Skipping all builds', GREEN)}")
        
        if not skip_test:
            print(f"\n{colored('Test Plan:', CYAN)}")
            print(f"  • {colored(f'Run {self.plugin_name} integration test', YELLOW)}")
        else:
            print(f"\n{colored('Test Plan:', CYAN)}")
            print(f"  • {colored('Skipping tests', GREEN)}")
        
        if dry_run:
            print(f"\n{colored('[DRY-RUN MODE]', MAGENTA)} No actual execution")
        
        # Execute builds in dependency order
        if not skip_build:
            # Stage 1: Raceboat code (required for stages 2 & 3)
            if build_raceboat_code:
                if not self.build_raceboat_code(dry_run):
                    return 1
            
            # Stage 2 & 3: Docker images (can run sequentially)
            if build_plugin_builder:
                if not self.build_plugin_builder_image(dry_run):
                    return 1
            
            if build_runtime:
                if not self.build_runtime_image(dry_run):
                    return 1
            
            # Plugin build (depends on plugin builder image)
            if build_plugin:
                if not self.build_plugin(dry_run):
                    return 1
        
        # Run test
        if not skip_test:
            # Clear logs before test
            self.clear_plugin_logs(dry_run)
            
            # Run integration test
            if not self.run_integration_test(config['wait_time'], dry_run):
                return 1
        
        # Success
        if not dry_run:
            self.print_header('✓ All Operations Completed Successfully')
        else:
            self.print_header('[DRY-RUN] All Operations Would Complete')
        
        return 0


def main():
    parser = argparse.ArgumentParser(
        description='Modular build-and-test orchestrator for raceboat plugins',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Test without rebuilding (using relative path)
  python3 build-test.py --plugin-dir ../../racebird
  
  # Rebuild plugin only, then test (using absolute path)
  python3 build-test.py --plugin-dir /path/to/my-plugin --rebuild-plugin
  
  # Rebuild raceboat only, then test
  python3 build-test.py --plugin-dir ../../../bluesky-transport --rebuild-raceboat
  
  # Rebuild everything
  python3 build-test.py --plugin-dir ../../racebird --rebuild-all
  
  # Granular rebuild
  python3 build-test.py --plugin-dir ../../racebird --rebuild-raceboat-code --rebuild-plugin
  
  # Dry-run mode
  python3 build-test.py --plugin-dir ../../racebird --rebuild-all --dry-run
  
  # Build only (skip test)
  python3 build-test.py --plugin-dir ../../racebird --rebuild-plugin --skip-test

Plugin Directory Structure:
  The plugin directory must contain:
    - build_artifacts_in_docker_image.sh  (build script)
    - test/setup.py                       (artifact setup script)
    - test/docker-compose.yml             (integration test config)
        """
    )
    
    # Required arguments
    parser.add_argument(
        '--plugin-dir',
        required=True,
        type=Path,
        help='Path to plugin directory (relative or absolute)'
    )
    parser.add_argument(
        '--raceboat-dir',
        type=Path,
        help='Path to raceboat directory (default: auto-detect from script location)'
    )
    
    # Rebuild options (hierarchical)
    rebuild_group = parser.add_argument_group('rebuild options')
    rebuild_group.add_argument(
        '--rebuild-all',
        action='store_true',
        help='Rebuild everything (raceboat + plugin)'
    )
    rebuild_group.add_argument(
        '--rebuild-raceboat',
        action='store_true',
        help='Rebuild all raceboat stages (code + images)'
    )
    rebuild_group.add_argument(
        '--rebuild-raceboat-code',
        action='store_true',
        help='Rebuild raceboat code only (Stage 1)'
    )
    rebuild_group.add_argument(
        '--rebuild-raceboat-images',
        action='store_true',
        help='Rebuild raceboat docker images (Stages 2 & 3)'
    )
    rebuild_group.add_argument(
        '--rebuild-plugin-builder',
        action='store_true',
        help='Rebuild plugin builder image only (Stage 2)'
    )
    rebuild_group.add_argument(
        '--rebuild-runtime-image',
        action='store_true',
        help='Rebuild runtime image only (Stage 3)'
    )
    rebuild_group.add_argument(
        '--rebuild-plugin',
        action='store_true',
        help='Rebuild specified plugin'
    )
    
    # Test options
    test_group = parser.add_argument_group('test options')
    test_group.add_argument(
        '--skip-test',
        action='store_true',
        help='Skip integration test (build only)'
    )
    test_group.add_argument(
        '--skip-build',
        action='store_true',
        help='Skip all builds (test only)'
    )
    test_group.add_argument(
        '--wait-time',
        type=int,
        default=10,
        help='Wait time for channel establishment (default: 10)'
    )
    
    # General options
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Show what would be done without executing'
    )
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Verbose output'
    )
    
    args = parser.parse_args()
    
    # Resolve plugin directory
    plugin_dir = args.plugin_dir.resolve()
    
    # Determine raceboat directory
    if args.raceboat_dir:
        raceboat_root = args.raceboat_dir.resolve()
    else:
        # Auto-detect: script is in raceboat/test/integration/
        script_dir = Path(__file__).parent.resolve()
        raceboat_root = script_dir.parent.parent
    
    if not raceboat_root.exists():
        print(f"{colored('Error:', RED)} Raceboat directory not found: {raceboat_root}")
        print("Use --raceboat-dir to specify the correct path")
        sys.exit(1)
    
    # Validate raceboat directory structure
    if not (raceboat_root / 'build.sh').exists():
        print(f"{colored('Error:', RED)} Not a valid raceboat directory: {raceboat_root}")
        print("Expected to find build.sh in raceboat directory")
        sys.exit(1)
    
    # Convert to config dict
    config = vars(args)
    
    try:
        # Execute orchestration
        orchestrator = BuildTestOrchestrator(
            raceboat_root=raceboat_root,
            plugin_dir=plugin_dir,
            verbose=args.verbose
        )
        exit_code = orchestrator.orchestrate(config)
        sys.exit(exit_code)
    except ValueError as e:
        print(f"\n{colored('Error:', RED)} {e}")
        print(f"\n{colored('Plugin directory requirements:', YELLOW)}")
        print("  - build_artifacts_in_docker_image.sh (at root)")
        print("  - test/setup.py")
        print("  - test/docker-compose.yml")
        sys.exit(1)


if __name__ == '__main__':
    main()
