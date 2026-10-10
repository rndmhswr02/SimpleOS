Set-Location $PSScriptRoot
& "$PSScriptRoot\build.ps1"
if ($LASTEXITCODE -ne 0) { exit 1 }

# -serial stdio : kernel's serial output shows up in this terminal
qemu-system-i386 -kernel build/kernel.elf -serial stdio