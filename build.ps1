$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

New-Item -ItemType Directory -Force -Path build | Out-Null

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

# 1. Assemble the entry stub with clang's built-in assembler (32-bit ELF)
Run "clang" @("--target=i686-elf", "-c", "src/boot.s", "-o", "build/boot.o")

# 2. Compile the kernel (freestanding: no standard library, no OS assumptions)
Run "clang" @(
    "--target=i686-elf",
    "-ffreestanding", "-fno-builtin", "-fno-stack-protector", "-fno-pic",
    "-nostdlib", "-O2", "-Wall", "-Wextra", "-std=c11",
    "-c", "src/kernel.c", "-o", "build/kernel.o"
)

# 3. Link everything using our linker script
Run "ld.lld" @(
    "-m", "elf_i386",
    "-T", "linker.ld",
    "-o", "build/kernel.elf",
    "build/boot.o", "build/kernel.o"
)

Write-Host "Build OK: build/kernel.elf" -ForegroundColor Green