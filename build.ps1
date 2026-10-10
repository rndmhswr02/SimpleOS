$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

New-Item -ItemType Directory -Force -Path build | Out-Null

# Remove old object files so deleted/renamed sources can't sneak into the link
Remove-Item build\*.o -ErrorAction SilentlyContinue

# Run a native command and stop on failure
function Run {
    param([string]$Exe, [string[]]$Arguments)
    Write-Host "> $Exe $($Arguments -join ' ')" -ForegroundColor DarkGray
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed." -ForegroundColor Red
        exit 1
    }
}

# Flags for all C files (freestanding: no standard library, no OS assumptions)
$cflags = @(
    "--target=i686-elf",
    "-ffreestanding", "-fno-builtin", "-fno-stack-protector", "-fno-pic",
    "-nostdlib", "-O2", "-Wall", "-Wextra", "-Werror", "-std=c11"
)

$objects = @()

# 1. Assemble the entry stub
Run "clang" @("--target=i686-elf", "-c", "src/boot.s", "-o", "build/boot.o")
$objects += "build/boot.o"

# 2. Compile every C file in src/
foreach ($src in Get-ChildItem src -Filter *.c) {
    $obj = "build/$($src.BaseName).o"
    Run "clang" ($cflags + @("-c", "src/$($src.Name)", "-o", $obj))
    $objects += $obj
}

# 3. Link everything using our linker script
Run "ld.lld" (@("-m", "elf_i386", "-T", "linker.ld", "-o", "build/kernel.elf") + $objects)

Write-Host "Build OK: build/kernel.elf" -ForegroundColor Green