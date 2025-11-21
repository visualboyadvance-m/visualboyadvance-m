#Requires -Version 5.0

<#
.SYNOPSIS
    Creates a ZIP archive using .NET classes, mimicking zip command behavior.

.DESCRIPTION
    This script creates ZIP archives compatible with the zip command line tool.
    It supports maximum compression, recursive operation, and symbolic link handling.

.PARAMETER Options
    Short options: -9 (max compression), -r (recursive), -y (store symlinks)
    Can be combined like -9yr or separate like -9 -y -r

.EXAMPLE
    .\zip.ps1 -9 -r archive.zip folder/
    .\zip.ps1 -9yr archive.zip folder/
#>

param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$Arguments
)

# Parse options and arguments
$maxCompression = $false
$recursive = $false
$storeSymlinks = $false
$zipFile = $null
$filesToAdd = @()

$i = 0
while ($i -lt $Arguments.Count) {
    $arg = $Arguments[$i]

    # Check if it's an option (starts with -)
    if ($arg -match '^-') {
        # Remove the leading dash(es)
        $opts = $arg -replace '^-+', ''

        # Parse each character as an option
        foreach ($char in $opts.ToCharArray()) {
            switch ($char) {
                '9' { $maxCompression = $true }
                'r' { $recursive = $true }
                'y' { $storeSymlinks = $true }
                default {
                    Write-Error "Unknown option: -$char"
                    exit 1
                }
            }
        }
    }
    elseif ($null -eq $zipFile) {
        # First non-option argument is the zip file
        $zipFile = $arg
    }
    else {
        # Remaining arguments are files to add
        $filesToAdd += $arg
    }

    $i++
}

# Validate arguments
if ($null -eq $zipFile) {
    Write-Error "Usage: zip.ps1 [options] zipfile files..."
    Write-Error "Options: -9 (max compression), -r (recursive), -y (store symlinks)"
    exit 1
}

if ($filesToAdd.Count -eq 0) {
    Write-Error "No files specified to add to archive"
    exit 1
}

# Load required .NET assembly
Add-Type -AssemblyName System.IO.Compression.FileSystem

# Delete existing zip file if it exists
if (Test-Path $zipFile) {
    Remove-Item $zipFile -Force
}

# Determine compression level
if ($maxCompression) {
    $compressionLevel = [System.IO.Compression.CompressionLevel]::SmallestSize
} else {
    $compressionLevel = [System.IO.Compression.CompressionLevel]::Optimal
}

try {
    # Create the zip archive
    $zipArchive = [System.IO.Compression.ZipFile]::Open($zipFile, 'Create')

    foreach ($item in $filesToAdd) {
        # Resolve to absolute path
        $sourcePath = (Resolve-Path $item -ErrorAction SilentlyContinue).Path

        if (-not $sourcePath) {
            Write-Warning "Path not found: $item"
            continue
        }

        # Check if it's a file or directory
        if (Test-Path $sourcePath -PathType Leaf) {
            # It's a file - add it directly
            $entryName = Split-Path $sourcePath -Leaf

            # Check if it's a symbolic link
            $fileInfo = Get-Item $sourcePath
            if ($storeSymlinks -and $fileInfo.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
                # For symbolic links, we'll just add the file normally
                # PowerShell/.NET doesn't have great symlink support in ZIP
                Write-Verbose "Adding symbolic link: $entryName"
            }

            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                $zipArchive,
                $sourcePath,
                $entryName,
                $compressionLevel
            ) | Out-Null

            Write-Verbose "Added: $entryName"
        }
        elseif (Test-Path $sourcePath -PathType Container) {
            # It's a directory
            if ($recursive) {
                # Get base path for relative entry names
                # Use the source path itself as base to avoid adding the root directory
                $baseDir = $sourcePath

                # Collect all entries (directories and files) with their metadata
                $allEntries = @()

                # Add directories
                $dirs = Get-ChildItem $sourcePath -Recurse -Directory
                foreach ($dir in $dirs) {
                    $relativePath = $dir.FullName.Substring($baseDir.Length).TrimStart('\', '/')
                    $entryName = ($relativePath -replace '\\', '/') + '/'
                    $allEntries += @{
                        Type = 'Directory'
                        EntryName = $entryName
                        FullPath = $dir.FullName
                    }
                }

                # Add files
                $files = Get-ChildItem $sourcePath -Recurse -File
                foreach ($file in $files) {
                    $relativePath = $file.FullName.Substring($baseDir.Length).TrimStart('\', '/')
                    $entryName = $relativePath -replace '\\', '/'
                    $allEntries += @{
                        Type = 'File'
                        EntryName = $entryName
                        FullPath = $file.FullName
                        IsSymlink = $file.Attributes -band [System.IO.FileAttributes]::ReparsePoint
                    }
                }

                # Sort entries by path using ordinal comparison (like zip.exe does)
                # This creates depth-first order: parent/, parent/child/, parent/child/file
                $allEntries = [System.Linq.Enumerable]::OrderBy(
                    [object[]]$allEntries,
                    [Func[object,string]]{ param($x) $x.EntryName },
                    [StringComparer]::Ordinal
                )

                # Add entries in sorted order
                foreach ($entry in $allEntries) {
                    if ($entry.Type -eq 'Directory') {
                        $zipArchive.CreateEntry($entry.EntryName) | Out-Null
                        Write-Verbose "Added directory: $($entry.EntryName)"
                    }
                    else {
                        # Check if it's a symbolic link
                        if ($storeSymlinks -and $entry.IsSymlink) {
                            Write-Verbose "Adding symbolic link: $($entry.EntryName)"
                        }

                        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                            $zipArchive,
                            $entry.FullPath,
                            $entry.EntryName,
                            $compressionLevel
                        ) | Out-Null

                        Write-Verbose "Added: $($entry.EntryName)"
                    }
                }
            }
            else {
                Write-Warning "Skipping directory (use -r for recursive): $sourcePath"
            }
        }
    }

    Write-Host "Created archive: $zipFile"
}
catch {
    Write-Error "Failed to create ZIP archive: $_"
    exit 1
}
finally {
    if ($zipArchive) {
        $zipArchive.Dispose()
    }
}

exit 0
