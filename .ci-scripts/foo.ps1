
[System.Reflection.Assembly]::LoadWithPartialName("System.Reflection.Metadata") | Out-Null
function Get-PEHeader {
    param([string]$FilePath)
    $fileStream = [System.IO.File]::OpenRead($FilePath)
    $reader = New-Object System.IO.BinaryReader($fileStream)
    $fileStream.Position = 0x3C
    $peOffset = $reader.ReadUInt32()
    $fileStream.Position = $peOffset + 4
    $machine = $reader.ReadUInt16()
    $fileStream.Close()

    switch ($machine) {
        0x014c { return "x86 (32-bit)" }
        0x8664 { return "x64 (64-bit)" }
        0x01c0 { return "ARM" }
        0x01c4 { return "ARMv7 (Thumb-2)" }
        0xAA64 { return "ARM64" }
        default { return "Unknown: 0x{0:X4}" -f $machine }
    }
}
Get-PEHeader -FilePath "C:\ponyc\bin\ponyc.exe"
