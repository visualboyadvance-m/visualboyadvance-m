#Requires -Version 5.0

<#
.SYNOPSIS
    Creates a ZIP archive using .NET classes, mimicking zip command behavior.

.DESCRIPTION
    This script creates ZIP archives compatible with the zip command line tool.
    It supports maximum compression, recursive operation, and symbolic link handling.

.PARAMETER Options
    Short options: -9 (max compression), -r (recursive), -y (store symlinks),
                   -j (junk directory names), -q (quiet operation)
    Can be combined like -9yrjq or separate like -9 -y -r -j -q

.EXAMPLE
    .\zip.ps1 -9 -r archive.zip folder/
    .\zip.ps1 -9yrjq archive.zip folder/
#>

param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$Arguments
)


# Parse options and arguments
$maxCompression = $false
$recursive = $false
$storeSymlinks = $false
$junkPaths = $false
$quiet = $false
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
                'j' { $junkPaths = $true }
                'q' { $quiet = $true }
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
    Write-Error "Options: -9 (max compression), -r (recursive), -y (store symlinks), -j (junk paths), -q (quiet)"
    exit 1
}

if ($filesToAdd.Count -eq 0) {
    Write-Error "No files specified to add to archive"
    exit 1
}

# Store original name for display purposes
$zipFileDisplay = $zipFile

# Ensure zip file path is absolute (relative to current directory, not user profile)
if (-not [System.IO.Path]::IsPathRooted($zipFile)) {
    $zipFile = Join-Path (Get-Location).Path $zipFile
}

# Load required .NET assembly
Add-Type -AssemblyName System.IO.Compression.FileSystem

# Helper function to add a file to a zip archive
# Using streams instead of ZipFileExtensions::CreateEntryFromFile for ps12exe compatibility
function Add-FileToZip {
    param(
        [System.IO.Compression.ZipArchive]$archive,
        [string]$filePath,
        [string]$entryName,
        [System.IO.Compression.CompressionLevel]$compression
    )

    $entry = $archive.CreateEntry($entryName, $compression)

    # Preserve the file's last write time
    $fileInfo = [System.IO.FileInfo]::new($filePath)
    $entry.LastWriteTime = $fileInfo.LastWriteTime

    $entryStream = $entry.Open()
    try {
        $fileStream = [System.IO.File]::OpenRead($filePath)
        try {
            $fileStream.CopyTo($entryStream)
        }
        finally {
            $fileStream.Dispose()
        }
    }
    finally {
        $entryStream.Dispose()
    }
}

# Delete existing zip file if it exists
if (Test-Path $zipFile) {
    Remove-Item $zipFile -Force
}

# Determine compression level
# Note: SmallestSize is not available in older .NET versions, Optimal is the best available
$compressionLevel = [System.IO.Compression.CompressionLevel]::Optimal

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
                if (-not $quiet) {
                    Write-Verbose "Adding symbolic link: $entryName"
                }
            }

            Add-FileToZip $zipArchive $sourcePath $entryName $compressionLevel

            if (-not $quiet) {
                Write-Verbose "Added: $entryName"
            }
        }
        elseif (Test-Path $sourcePath -PathType Container) {
            # It's a directory
            if ($recursive) {
                # Get base path for relative entry names
                # Use the source path itself as base to avoid adding the root directory
                $baseDir = $sourcePath

                # Collect all entries (directories and files) with their metadata
                $allEntries = @()

                # Add directories (only if not junking paths)
                if (-not $junkPaths) {
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
                }

                # Add files
                $files = Get-ChildItem $sourcePath -Recurse -File
                foreach ($file in $files) {
                    if ($junkPaths) {
                        # Just use the filename, no directory path
                        $entryName = $file.Name
                    } else {
                        $relativePath = $file.FullName.Substring($baseDir.Length).TrimStart('\', '/')
                        $entryName = $relativePath -replace '\\', '/'
                    }
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
                        if (-not $quiet) {
                            Write-Verbose "Added directory: $($entry.EntryName)"
                        }
                    }
                    else {
                        # Check if it's a symbolic link
                        if ($storeSymlinks -and $entry.IsSymlink) {
                            if (-not $quiet) {
                                Write-Verbose "Adding symbolic link: $($entry.EntryName)"
                            }
                        }

                        Add-FileToZip $zipArchive $entry.FullPath $entry.EntryName $compressionLevel

                        if (-not $quiet) {
                            Write-Verbose "Added: $($entry.EntryName)"
                        }
                    }
                }
            }
            else {
                if (-not $quiet) {
                    Write-Warning "Skipping directory (use -r for recursive): $sourcePath"
                }
            }
        }
    }

    if (-not $quiet) {
        Write-Host "Created archive: $zipFileDisplay"
    }
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
