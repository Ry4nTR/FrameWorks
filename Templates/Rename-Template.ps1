g<#
.SYNOPSIS
  Copies the Public/ and Private/ folders of the starter kit into a game module and renames MyGame to the project name.

.DESCRIPTION
  Text and file names are changed in this order (the longer names first, the bare token last):
    MyGameMode          -> <ProjectName>GameMode
    MyGameInstance      -> <ProjectName>GameInstance
    MyPlayerController  -> <ProjectName>PlayerController
    MYGAME              -> <PROJECTNAME>          (the export macro MYGAME_API)
    MyGame              -> <ProjectName>          (log category, folders in comments, Category names)
  The script refuses to overwrite an existing file and writes nothing if any target already exists.
  It does not touch MyGame.Build.cs: see Templates/README.md, step 3.

  STATUS: not run. Written without PowerShell available to the author. Read it once before the first use.

.PARAMETER ProjectName
  Name of your game module, letters and digits only, starting with a letter. It must not contain "MyGame".

.PARAMETER ModuleDirectory
  Folder of the game module, for example <ProjectRoot>\Source\<ProjectName>. It must already exist.

.EXAMPLE
  .\Rename-Template.ps1 -ProjectName Atlas -ModuleDirectory ..\..\..\Source\Atlas
#>

[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[ValidatePattern('^[A-Za-z][A-Za-z0-9]*$')]
	[string]$ProjectName,

	[Parameter(Mandatory = $true)]
	[string]$ModuleDirectory
)

$ErrorActionPreference = 'Stop'

if ($ProjectName -match 'mygame') {
	throw "ProjectName must not contain 'MyGame' (the replacement would run twice)."
}

if (-not (Test-Path -LiteralPath $ModuleDirectory -PathType Container)) {
	throw "Module directory not found: $ModuleDirectory"
}

$moduleRoot = (Resolve-Path -LiteralPath $ModuleDirectory).Path
$upperName = $ProjectName.ToUpperInvariant()

function ConvertTo-ProjectText {
	param([string]$Text)

	$Text = $Text.Replace('MyGameMode', "${ProjectName}GameMode")
	$Text = $Text.Replace('MyGameInstance', "${ProjectName}GameInstance")
	$Text = $Text.Replace('MyPlayerController', "${ProjectName}PlayerController")
	$Text = $Text.Replace('MYGAME', $upperName)
	$Text = $Text.Replace('MyGame', $ProjectName)
	return $Text
}

# Pass 1: build the plan and check every target before writing anything.
$plan = @()
foreach ($folder in @('Public', 'Private')) {
	$sourceRoot = Join-Path $PSScriptRoot $folder
	foreach ($file in Get-ChildItem -LiteralPath $sourceRoot -Recurse -File) {
		$relative = $file.FullName.Substring($sourceRoot.Length).TrimStart('\', '/')
		$target = Join-Path (Join-Path $moduleRoot $folder) (ConvertTo-ProjectText $relative)
		if (Test-Path -LiteralPath $target) {
			throw "Refusing to overwrite an existing file: $target"
		}
		$plan += [pscustomobject]@{ Source = $file.FullName; Target = $target }
	}
}

# Pass 2: write the files (UTF-8 without BOM).
$encoding = New-Object System.Text.UTF8Encoding($false)
foreach ($item in $plan) {
	$targetFolder = Split-Path -Parent $item.Target
	New-Item -ItemType Directory -Force -Path $targetFolder | Out-Null
	$text = [System.IO.File]::ReadAllText($item.Source)
	[System.IO.File]::WriteAllText($item.Target, (ConvertTo-ProjectText $text), $encoding)
	Write-Host "Created $($item.Target)"
}

Write-Host ""
Write-Host "Done: $($plan.Count) files. Next: README step 3 (Build.cs), then regenerate the project files and build."
