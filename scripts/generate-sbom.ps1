param(
  [Parameter(Mandatory = $true)]
  [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$packages = @(
  @{
    SPDXID = 'SPDXRef-Package-cyan-cpp'
    name = 'cyan-cpp'
    versionInfo = '0.3.0'
    downloadLocation = 'NOASSERTION'
    filesAnalyzed = $false
    licenseConcluded = 'MIT'
    licenseDeclared = 'MIT'
    copyrightText = 'Copyright (c) 2026 cyan-cpp contributors'
  },
  @{
    SPDXID = 'SPDXRef-Package-LIEF'
    name = 'LIEF'
    versionInfo = '0.17.6'
    downloadLocation = 'https://github.com/lief-project/LIEF/archive/refs/tags/0.17.6.tar.gz'
    filesAnalyzed = $false
    licenseConcluded = 'Apache-2.0'
    licenseDeclared = 'Apache-2.0'
    copyrightText = 'NOASSERTION'
  },
  @{
    SPDXID = 'SPDXRef-Package-libarchive'
    name = 'libarchive'
    versionInfo = '3.8.7'
    downloadLocation = 'https://github.com/libarchive/libarchive/archive/refs/tags/v3.8.7.tar.gz'
    filesAnalyzed = $false
    licenseConcluded = 'BSD-2-Clause'
    licenseDeclared = 'BSD-2-Clause'
    copyrightText = 'NOASSERTION'
  },
  @{
    SPDXID = 'SPDXRef-Package-libzip'
    name = 'libzip'
    versionInfo = '1.11.4'
    downloadLocation = 'https://github.com/nih-at/libzip/archive/refs/tags/v1.11.4.tar.gz'
    filesAnalyzed = $false
    licenseConcluded = 'BSD-3-Clause'
    licenseDeclared = 'BSD-3-Clause'
    copyrightText = 'NOASSERTION'
  },
  @{
    SPDXID = 'SPDXRef-Package-nlohmann-json'
    name = 'nlohmann-json'
    versionInfo = '3.12.0'
    downloadLocation = 'https://github.com/nlohmann/json/archive/refs/tags/v3.12.0.tar.gz'
    filesAnalyzed = $false
    licenseConcluded = 'MIT'
    licenseDeclared = 'MIT'
    copyrightText = 'NOASSERTION'
  },
  @{
    SPDXID = 'SPDXRef-Package-ldid'
    name = 'ldid'
    versionInfo = '2.1.5-procursus7+c50e84e'
    downloadLocation = 'https://github.com/ProcursusTeam/ldid/tree/c50e84e18532044b23ec5e971d55ab0cdd4b6685'
    filesAnalyzed = $false
    licenseConcluded = 'AGPL-3.0-only'
    licenseDeclared = 'AGPL-3.0-only'
    copyrightText = 'Copyright ldid contributors'
  }
)

$relationships = @(
  @{
    spdxElementId = 'SPDXRef-DOCUMENT'
    relationshipType = 'DESCRIBES'
    relatedSpdxElement = 'SPDXRef-Package-cyan-cpp'
  },
  @{
    spdxElementId = 'SPDXRef-Package-cyan-cpp'
    relationshipType = 'DEPENDS_ON'
    relatedSpdxElement = 'SPDXRef-Package-LIEF'
  },
  @{
    spdxElementId = 'SPDXRef-Package-cyan-cpp'
    relationshipType = 'DEPENDS_ON'
    relatedSpdxElement = 'SPDXRef-Package-libarchive'
  },
  @{
    spdxElementId = 'SPDXRef-Package-cyan-cpp'
    relationshipType = 'DEPENDS_ON'
    relatedSpdxElement = 'SPDXRef-Package-libzip'
  },
  @{
    spdxElementId = 'SPDXRef-Package-cyan-cpp'
    relationshipType = 'DEPENDS_ON'
    relatedSpdxElement = 'SPDXRef-Package-nlohmann-json'
  },
  @{
    spdxElementId = 'SPDXRef-Package-cyan-cpp'
    relationshipType = 'DEPENDS_ON'
    relatedSpdxElement = 'SPDXRef-Package-ldid'
  }
)

$document = @{
  spdxVersion = 'SPDX-2.3'
  dataLicense = 'CC0-1.0'
  SPDXID = 'SPDXRef-DOCUMENT'
  name = 'cyan-cpp-windows-x64'
  documentNamespace = "https://github.com/cyan-cpp/sbom/$([guid]::NewGuid())"
  creationInfo = @{
    created = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
    creators = @('Tool: cyan-cpp/scripts/generate-sbom.ps1')
  }
  packages = $packages
  relationships = $relationships
}

$json = $document | ConvertTo-Json -Depth 8
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText(
  [System.IO.Path]::GetFullPath($OutputPath),
  "$json`n",
  $utf8NoBom
)
