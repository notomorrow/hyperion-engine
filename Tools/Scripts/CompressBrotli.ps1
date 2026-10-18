#Requires -Version 7.0
param(
    [Parameter(Mandatory = $true)][string]$Directory,
    [int]$MinimumSize = 64KB,
    [int]$Quality = 11
)

# Replaces each large .bin in a web build's Cache folder with <name>.br, and writes brotli.json listing them with their
# sizes, which the page reads to fetch and decode them.

$ErrorActionPreference = "Stop"

Add-Type -TypeDefinition @"
public static class HypBrotli
{
    public static byte[] Compress(byte[] source, int quality, int window)
    {
        byte[] destination = new byte[System.IO.Compression.BrotliEncoder.GetMaxCompressedLength(source.Length)];

        if (!System.IO.Compression.BrotliEncoder.TryCompress(source, destination, out int written, quality, window))
        {
            throw new System.InvalidOperationException("Brotli compression failed");
        }

        System.Array.Resize(ref destination, written);

        return destination;
    }
}
"@

$entries = [ordered]@{}

foreach ($file in Get-ChildItem $Directory -File -Filter *.bin | Sort-Object Name)
{
    if ($file.Length -lt $MinimumSize)
    {
        continue;
    }

    $compressed = [HypBrotli]::Compress([System.IO.File]::ReadAllBytes($file.FullName), $Quality, 24)

    [System.IO.File]::WriteAllBytes("$($file.FullName).br", $compressed)
    Remove-Item $file.FullName

    $entries[$file.Name] = [ordered]@{ size = $file.Length; compressedSize = $compressed.Length }

    Write-Host ("  {0}: {1:N1} MB -> {2:N1} MB" -f $file.Name, ($file.Length / 1MB), ($compressed.Length / 1MB))
}

$entries | ConvertTo-Json | Set-Content (Join-Path $Directory "brotli.json") -Encoding ascii
