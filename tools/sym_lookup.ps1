param(
    [Parameter(Mandatory = $true)][string]$ModulePath,
    [Parameter(Mandatory = $true)][string]$SearchPath,
    [Parameter(Mandatory = $true)][UInt32]$Base,
    [Parameter(Mandatory = $true)][UInt32]$Size,
    [Parameter(ValueFromRemainingArguments = $true)][UInt32[]]$Addresses
)

$src = @'
using System;
using System.Runtime.InteropServices;

public static class DbgHelpNative
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct SYMBOL_INFO
    {
        public uint SizeOfStruct;
        public uint TypeIndex;
        public ulong Reserved1;
        public ulong Reserved2;
        public uint Index;
        public uint Size;
        public ulong ModBase;
        public uint Flags;
        public ulong Value;
        public ulong Address;
        public uint Register;
        public uint Scope;
        public uint Tag;
        public uint NameLen;
        public uint MaxNameLen;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2048)]
        public byte[] Name;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct IMAGEHLP_LINE64
    {
        public uint SizeOfStruct;
        public IntPtr Key;
        public uint LineNumber;
        public IntPtr FileName;
        public ulong Address;
    }

    [DllImport("dbghelp.dll", SetLastError = true)]
    public static extern bool SymInitialize(IntPtr hProcess, string UserSearchPath, bool fInvadeProcess);

    [DllImport("dbghelp.dll", SetLastError = true)]
    public static extern uint SymSetOptions(uint SymOptions);

    [DllImport("dbghelp.dll", SetLastError = true)]
    public static extern ulong SymLoadModuleEx(IntPtr hProcess, IntPtr hFile, string ImageName, string ModuleName,
        ulong BaseOfDll, uint DllSize, IntPtr Data, uint Flags);

    [DllImport("dbghelp.dll", SetLastError = true)]
    public static extern bool SymFromAddr(IntPtr hProcess, ulong Address, out ulong Displacement, ref SYMBOL_INFO Symbol);

    [DllImport("dbghelp.dll", SetLastError = true)]
    public static extern bool SymGetLineFromAddr64(IntPtr hProcess, ulong dwAddr, out uint pdwDisplacement, ref IMAGEHLP_LINE64 Line);
}
'@

Add-Type -TypeDefinition $src -Language CSharp

# SYMOPT_UNDNAME(0x2) | SYMOPT_DEFERRED_LOADS(0x4) | SYMOPT_LOAD_LINES(0x10) | SYMOPT_DEBUG(0x80000000)
[DbgHelpNative]::SymSetOptions([uint32]"0x80000016") | Out-Null
$fakeProcess = [IntPtr]::new(0x1234)
if (-not [DbgHelpNative]::SymInitialize($fakeProcess, $SearchPath, $false)) {
    Write-Output "SymInitialize failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    exit 1
}

$loaded = [DbgHelpNative]::SymLoadModuleEx($fakeProcess, [IntPtr]::Zero, $ModulePath, $null, [uint64]$Base, $Size, [IntPtr]::Zero, 0)
if ($loaded -eq 0) {
    Write-Output "SymLoadModuleEx failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
}
Write-Output ("loaded module base 0x{0:X}" -f $loaded)

foreach ($a in $Addresses) {
    $sym = New-Object DbgHelpNative+SYMBOL_INFO
    if ([IntPtr]::Size -eq 8) { $sym.SizeOfStruct = 88 } else { $sym.SizeOfStruct = 84 }
    $sym.MaxNameLen = 2047
    $sym.Name = New-Object byte[] 2048
    [uint64]$disp = 0
    $ok = [DbgHelpNative]::SymFromAddr($fakeProcess, [uint64]$a, [ref]$disp, [ref]$sym)
    if ($ok) {
        $name = [Text.Encoding]::ASCII.GetString($sym.Name, 0, [int]$sym.NameLen)
        Write-Output ("0x{0:X8}: {1} + 0x{2:X}" -f $a, $name, $disp)
    }
    else {
        Write-Output ("0x{0:X8}: <no symbol> err={1}" -f $a, [Runtime.InteropServices.Marshal]::GetLastWin32Error())
    }

    $line = New-Object DbgHelpNative+IMAGEHLP_LINE64
    $line.SizeOfStruct = [uint32]([Runtime.InteropServices.Marshal]::SizeOf([type][DbgHelpNative+IMAGEHLP_LINE64]) - 8 + 4)
    [uint32]$ldisp = 0
    if ([DbgHelpNative]::SymGetLineFromAddr64($fakeProcess, [uint64]$a, [ref]$ldisp, [ref]$line)) {
        $file = [Runtime.InteropServices.Marshal]::PtrToStringAnsi($line.FileName)
        Write-Output ("    {0}:{1}" -f $file, $line.LineNumber)
    }
}
