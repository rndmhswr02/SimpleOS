$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

New-Item -ItemType Directory -Force -Path build, build/user | Out-Null

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
    "-mno-sse", "-mno-mmx", "-mno-sse2",
    "-nostdlib", "-O2", "-Wall", "-Wextra", "-Werror", "-std=c11"
)

# 0. User programs -> flat binaries at 0x40000000 (the kernel embeds them below)
foreach ($src in Get-ChildItem user -Filter *.c) {
    $name = $src.BaseName
    Run "clang" ($cflags + @("-c", "user/$($src.Name)", "-o", "build/user/$name.o"))
    Run "ld.lld" @("-m", "elf_i386", "-T", "user.ld", "--oformat=binary",
                   "-o", "build/user/$name.bin", "build/user/$name.o")
}

$objects = @()

# 1. Assemble every .s file in src/ (userprogs.s pastes in the binaries from step 0)
foreach ($src in Get-ChildItem src -Filter *.s) {
    $obj = "build/$($src.Name).o"
    Run "clang" @("--target=i686-elf", "-c", "src/$($src.Name)", "-o", $obj)
    $objects += $obj
}

# 2. Compile every .c file in src/
foreach ($src in Get-ChildItem src -Filter *.c) {
    $obj = "build/$($src.Name).o"
    Run "clang" ($cflags + @("-c", "src/$($src.Name)", "-o", $obj))
    $objects += $obj
}

# 3. Link everything using our linker script
Run "ld.lld" (@("-m", "elf_i386", "-T", "linker.ld", "-o", "build/kernel.elf") + $objects)

Write-Host "Build OK: build/kernel.elf" -ForegroundColor Green